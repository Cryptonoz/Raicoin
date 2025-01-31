#include <rai/rai_token/token.hpp>

#include <rai/common/parameters.hpp>

rai::TokenError::TokenError() : return_code_(rai::ErrorCode::SUCCESS)
{
}

rai::TokenError::TokenError(rai::ErrorCode error_code)
    : return_code_(error_code)
{
}

rai::TokenError::TokenError(rai::ErrorCode error_code,
                            const rai::TokenKey& token)
    : return_code_(error_code), token_(token)
{
}

rai::Token::Token(rai::ErrorCode& error_code, boost::asio::io_service& service,
                  const boost::filesystem::path& data_path, rai::Alarm& alarm,
                  const rai::TokenConfig& config)
    : App(error_code, service, data_path / "token_data.ldb", alarm, config.app_,
          subscribe_, rai::Token::BlockTypes(), rai::Token::Provide()),
      service_(service),
      alarm_(alarm),
      config_(config),
      subscribe_(*this)
{
    IF_NOT_SUCCESS_RETURN_VOID(error_code);
    error_code = InitLedger_();
}

rai::ErrorCode rai::Token::PreBlockAppend(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    bool confirmed)
{
    if (confirmed)
    {
        return rai::ErrorCode::SUCCESS;
    }

    do
    {
        if (block->Extensions().empty())
        {
            break;
        }

        rai::Extensions extensions;
        rai::ErrorCode error_code = extensions.FromBytes(block->Extensions());
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            break;
        }

        if (extensions.Count(rai::ExtensionType::TOKEN) > 0)
        {
            return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
        }
    } while (0);

    rai::Account account = block->Account();
    uint64_t height = block->Height();
    if (ledger_.InquiryWaitingExist(transaction, account, height))
    {
        return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
    }

    if (ledger_.TakeWaitingExist(transaction, account, height))
    {
        return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::AfterBlockAppend(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    bool confirmed)
{
    do
    {
        if (block->Extensions().empty())
        {
            break;
        }

        rai::Extensions extensions;
        rai::ErrorCode error_code = extensions.FromBytes(block->Extensions());
        if (error_code != rai::ErrorCode::SUCCESS)
        {
            break;
        }

        if (extensions.Count(rai::ExtensionType::TOKEN) == 0)
        {
            break;
        }

        if (!confirmed)
        {
            rai::Log::Error(rai::ToString(
                "Alias::AfterBlockAppend: unexpected unconfirmed block, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
        }

        error_code =  Process(transaction, block, extensions);
        IF_NOT_SUCCESS_RETURN(error_code);
    } while (0);
    
    rai::Account account = block->Account();
    uint64_t height = block->Height();
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    if (ledger_.InquiryWaitingExist(transaction, account, height))
    {
        if (!confirmed)
        {
            rai::Log::Error(rai::ToString(
                "Alias::AfterBlockAppend: unexpected unconfirmed block, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
        }
        error_code = PurgeInquiryWaiting_(transaction, account, height);
        IF_NOT_SUCCESS_RETURN(error_code);
    }

    if (ledger_.TakeWaitingExist(transaction, account, height))
    {
        if (!confirmed)
        {
            rai::Log::Error(rai::ToString(
                "Alias::AfterBlockAppend: unexpected unconfirmed block, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_CONFIRM_REQUIRED;
        }
        error_code = PurgeTakeWaiting_(transaction, block);
        IF_NOT_SUCCESS_RETURN(error_code);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::PreBlockRollback(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    rai::AccountTokensInfo info;
    bool error =
        ledger_.AccountTokensInfoGet(transaction, block->Account(), info);
    IF_ERROR_RETURN(error, rai::ErrorCode::SUCCESS);

    if (info.head_ != rai::Block::INVALID_HEIGHT
        && info.head_ >= block->Height())
    {
        std::string error_info = rai::ToString(
            "Token::PreBlockRollback: rollback confirmed block, hash=",
            block->Hash().StringHex());
        rai::Log::Error(error_info);
        std::cout << "FATAL ERROR:" << error_info << std::endl;
        return rai::ErrorCode::APP_PROCESS_HALT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::AfterBlockRollback(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::Process(rai::Transaction& transaction,
                                   const std::shared_ptr<rai::Block>& block,
                                   const rai::Extensions& extensions)
{
    rai::Account account = block->Account();
    uint64_t height = block->Height();
    rai::AccountTokensInfo info;
    bool error = ledger_.AccountTokensInfoGet(transaction, account, info);
    if (!error && info.head_ >= height)
    {
        rai::Log::Error(rai::ToString("Token::Process: block re-entry, hash=",
                                      block->Hash().StringHex()));
        return rai::ErrorCode::SUCCESS;
    }

    if (extensions.Count(rai::ExtensionType::TOKEN) != 1)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_MULTI_EXTENSIONS);
    }

    rai::ExtensionToken token;
    rai::ErrorCode error_code =
        token.FromExtension(extensions.Get(rai::ExtensionType::TOKEN));
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        return UpdateLedgerCommon_(transaction, block, error_code);
    }

    if (token.op_data_ == nullptr)
    {
        rai::Log::Error(
            rai::ToString("Token::Process: op_data is null, hash=",
                            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    using Op = rai::ExtensionToken::Op;
    rai::TokenError token_error;
    switch (token.op_)
    {
        case Op::CREATE:
        {
            token_error = ProcessCreate_(transaction, block, token);
            break;
        }
        case Op::MINT:
        {
            token_error = ProcessMint_(transaction, block, token);
            break;
        }
        case Op::BURN:
        {
            token_error = ProcessBurn_(transaction, block, token);
            break;
        }
        case Op::SEND:
        {
            token_error = ProcessSend_(transaction, block, token);
            break;
        }
        case Op::RECEIVE:
        {
            token_error = ProcessReceive_(transaction, block, token);
            break;
        }
        case Op::SWAP:
        {
            token_error = ProcessSwap_(transaction, block, token);
            break;
        }
        case Op::UNMAP:
        {
            token_error = ProcessUnmap_(transaction, block, token);
            break;
        }
        case Op::WRAP:
        {
            token_error = ProcessWrap_(transaction, block, token);
            break;
        }
        default:
        {
            rai::Log::Error(
                rai::ToString("Token::Process: unknown token operation, hash=",
                              block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }
    }

    error_code = ProcessError_(transaction, token_error);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

std::shared_ptr<rai::AppRpcHandler> rai::Token::MakeRpcHandler(
    const rai::UniqueId& uid, bool check, const std::string& body,
    const std::function<void(const rai::Ptree&)>& send_response)
{
    if (!rpc_)
    {
        return nullptr;
    }
    return std::make_shared<rai::TokenRpcHandler>(
        *this, uid, check, *rpc_, body, boost::asio::ip::address_v4::any(),
        send_response);
}

std::shared_ptr<rai::Token> rai::Token::Shared()
{
    return Shared_<rai::Token>();
}

rai::RpcHandlerMaker rai::Token::RpcHandlerMaker()
{
    return [this](rai::Rpc& rpc, const std::string& body,
                  const boost::asio::ip::address_v4& ip,
                  const std::function<void(const rai::Ptree&)>& send_response)
               -> std::unique_ptr<rai::RpcHandler> {
        return std::make_unique<rai::TokenRpcHandler>(
            *this, rai::UniqueId(), false, rpc, body, ip, send_response);
    };
}

void rai::Token::Start()
{
    std::weak_ptr<rai::Token> token(Shared());
    receivable_observer_ = [token](const rai::TokenReceivableKey& key,
                                   const rai::TokenReceivable& receivable) {
        auto token_s = token.lock();
        if (token_s == nullptr) return;

        token_s->Background([token, key, receivable]() {
            if (auto token_s = token.lock())
            {
                token_s->observers_.receivable_.Notify(key, receivable);
            }
        });
    };

    token_creation_observer_ = [token](const rai::TokenKey& key) {
        auto token_s = token.lock();
        if (token_s == nullptr) return;

        token_s->Background([token, key]() {
            if (auto token_s = token.lock())
            {
                token_s->observers_.token_creation_.Notify(key);
            }
        });
    };

    App::Start();
}

void rai::Token::Stop()
{
    if (rpc_)
    {
        rpc_->Stop();
    }

    if (runner_)
    {
        runner_->Stop();
    }

    App::Stop();
}

std::vector<rai::BlockType> rai::Token::BlockTypes()
{
    std::vector<rai::BlockType> types{rai::BlockType::TX_BLOCK,
                                      rai::BlockType::REP_BLOCK};
    return types;
}

rai::Provider::Info rai::Token::Provide()
{
    using P = rai::Provider;
    P::Info info;
    info.id_ = P::Id::TOKEN;

    info.filters_.push_back(P::Filter::APP_ACCOUNT);

    info.actions_.push_back(P::Action::APP_SERVICE_SUBSCRIBE);
    info.actions_.push_back(P::Action::APP_ACCOUNT_SYNC);
    // todo:

    return info;
}

rai::ErrorCode rai::Token::InitLedger_()
{
    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    rai::Transaction transaction(error_code, ledger_, true);
    IF_NOT_SUCCESS_RETURN(error_code);

    uint32_t version = 0;
    bool error = ledger_.VersionGet(transaction, version);
    if (error)
    {
        error =
            ledger_.VersionPut(transaction, rai::Token::CURRENT_LEDGER_VERSION);
        IF_ERROR_RETURN(error, rai::ErrorCode::TOKEN_LEDGER_PUT);
        return rai::ErrorCode::SUCCESS;
    }

    if (version > rai::Token::CURRENT_LEDGER_VERSION)
    {
        std::cout << "[Error] Ledger version=" << version << std::endl;
        return rai::ErrorCode::LEDGER_VERSION;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessCreate_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    rai::TokenKey key(rai::CurrentChain(), block->Account());
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, key, info);
    if (!error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_RECREATE, keys);
    }

    auto create =
        std::static_pointer_cast<rai::ExtensionTokenCreate>(extension.op_data_);
    rai::ErrorCode error_code = create->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessCreate_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (create->creation_data_ == nullptr)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessCreate_: creation data is null, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (create->type_ == rai::TokenType::_20)
    {
        auto create20 = std::static_pointer_cast<rai::ExtensionToken20Create>(
            create->creation_data_);
        rai::TokenInfo info(
            rai::TokenType::_20, create20->symbol_, create20->name_,
            create20->decimals_, create20->burnable_, create20->mintable_,
            create20->circulable_, create20->cap_supply_, block->Height());
        if (create20->init_supply_ > 0)
        {
            info.total_supply_ = create20->init_supply_;
            info.local_supply_ = create20->init_supply_;
            rai::TokenReceivableKey receivable_key(
                block->Account(), key, rai::CurrentChain(), block->Hash());
            rai::TokenReceivable receivable(
                block->Account(), create20->init_supply_, block->Height(),
                rai::TokenType::_20, rai::TokenSource::MINT);
            error_code = UpdateLedgerReceivable_(transaction, receivable_key,
                                                 receivable);
            IF_NOT_SUCCESS_RETURN(error_code);
        }

        error = ledger_.TokenInfoPut(transaction, key, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessCreate_: put token info, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }
    else if (create->type_ == rai::TokenType::_721)
    {
        auto create721 = std::static_pointer_cast<rai::ExtensionToken721Create>(
            create->creation_data_);
        rai::TokenInfo info(rai::TokenType::_721, create721->symbol_,
                            create721->name_, create721->burnable_,
                            create721->circulable_, create721->cap_supply_,
                            create721->base_uri_, block->Height());
        error = ledger_.TokenInfoPut(transaction, key, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessCreate_: put token info, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }
    else
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessCreate_: unexpected token type, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (token_creation_observer_)
    {
        token_creation_observer_(key);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessMint_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    rai::TokenKey key(rai::CurrentChain(), block->Account());
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_NOT_CREATED);
    }

    if (!info.mintable_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_UNMINTABLE, keys);
    }

    auto mint =
        std::static_pointer_cast<rai::ExtensionTokenMint>(extension.op_data_);
    rai::ErrorCode error_code = mint->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessMint_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (mint->type_ != info.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }

    rai::TokenValue total_supply(0);
    rai::TokenValue local_supply(0);
    if (mint->type_ == rai::TokenType::_20)
    {
        total_supply = info.total_supply_ + mint->value_;
        if (total_supply <= info.total_supply_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_TOTAL_SUPPLY_OVERFLOW,
                keys);
        }

        if (info.cap_supply_ > 0 && total_supply > info.cap_supply_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_CAP_SUPPLY_EXCEEDED,
                keys);
        }

        local_supply = info.local_supply_ + mint->value_;
        if (local_supply <= info.local_supply_)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessMint_: local supply overflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        rai::TokenReceivableKey receivable_key(
            mint->to_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(
            block->Account(), mint->value_, block->Height(),
            rai::TokenType::_20, rai::TokenSource::MINT);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else if (mint->type_ == rai::TokenType::_721)
    {
        total_supply = info.total_supply_ + 1;
        if (total_supply <= info.total_supply_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_TOTAL_SUPPLY_OVERFLOW,
                keys);
        }

        if (info.cap_supply_ > 0 && total_supply > info.cap_supply_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_CAP_SUPPLY_EXCEEDED,
                keys);
        }

        local_supply = info.local_supply_ + 1;
        if (local_supply <= info.local_supply_)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessMint_: local supply overflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        rai::TokenIdInfo token_id_info;
        error = ledger_.TokenIdInfoGet(transaction, key, mint->value_,
                                       token_id_info);
        if (!error && !token_id_info.burned_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_REMINT,
                keys);
        }

        if (error)
        {
            token_id_info = rai::TokenIdInfo(mint->uri_);
        }
        else
        {
            token_id_info.burned_ = false;
            token_id_info.uri_ = mint->uri_;
        }

        error = ledger_.TokenIdInfoPut(transaction, key, mint->value_,
                                       token_id_info);
        if (error)
        {
            rai::Log::Error(
                rai::ToString("Token::ProcessMint_: put token id info, hash=",
                              block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        rai::TokenReceivableKey receivable_key(
            mint->to_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(block->Account(), mint->value_,
                                        block->Height(), rai::TokenType::_721,
                                        rai::TokenSource::MINT);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessMint_: unexpected token type, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error_code =
        UpdateLedgerSupplies_(transaction, key, total_supply, local_supply);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessBurn_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    rai::TokenKey key(rai::CurrentChain(), block->Account());
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_NOT_CREATED);
    }

    if (!info.burnable_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_UNBURNABLE, keys);
    }

    auto burn =
        std::static_pointer_cast<rai::ExtensionTokenBurn>(extension.op_data_);
    rai::ErrorCode error_code = burn->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessBurn_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (burn->type_ != info.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }

    rai::AccountTokenInfo account_token_info;
    error = ledger_.AccountTokenInfoGet(transaction, block->Account(),
                                        rai::CurrentChain(), block->Account(),
                                        account_token_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessBurn_: get account token info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    rai::TokenValue balance(account_token_info.balance_);
    rai::TokenValue total_supply(0);
    rai::TokenValue local_supply(0);
    if (burn->type_ == rai::TokenType::_20)
    {
        if (burn->value_ > account_token_info.balance_)
        {
            return UpdateLedgerCommon_(
                transaction, block,
                rai::ErrorCode::TOKEN_BURN_MORE_THAN_BALANCE, keys);
        }
        balance = account_token_info.balance_ - burn->value_;

        if (burn->value_ > info.total_supply_)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: burn more than total supply, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        total_supply = info.total_supply_ - burn->value_;

        if (burn->value_ > info.local_supply_)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: burn more than local supply, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        local_supply = info.local_supply_ - burn->value_;
    }
    else if (burn->type_ == rai::TokenType::_721)
    {
        rai::TokenValue token_id = burn->value_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        if (!ledger_.AccountTokenIdExist(transaction, account_token_id))
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_NOT_OWNED, keys);
        }

        if (account_token_info.balance_ < 1)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: account token balance underflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        balance = account_token_info.balance_ - 1;

        if (info.total_supply_ < 1)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: token total supply underflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        total_supply = info.total_supply_ - 1;

        if (info.local_supply_ < 1)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: token local supply underflow, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT; // Ledger inconsistency
        }
        local_supply = info.local_supply_ - 1;

        rai::TokenIdInfo token_id_info;
        error =
            ledger_.TokenIdInfoGet(transaction, key, token_id, token_id_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: get token id info failed, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT; // Ledger inconsistency
        }
        token_id_info.burned_ = true;
        error =
            ledger_.TokenIdInfoPut(transaction, key, token_id, token_id_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessBurn_: put token id info failed, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        error = ledger_.AccountTokenIdDel(transaction, account_token_id);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessMint_: del account token id, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }

        error_code = UpdateLedgerTokenIdTransfer_(
            transaction, key, burn->value_, block->Account(), block->Height());
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessBurn_: unexpected token type, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error_code =
        UpdateLedgerTokenTransfer_(transaction, key, block->Timestamp(),
                                   block->Account(), block->Height());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerBalance_(transaction, block->Account(), key,
                             account_token_info.balance_, balance);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerSupplies_(transaction, key, total_supply, local_supply);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSend_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    auto send =
        std::static_pointer_cast<rai::ExtensionTokenSend>(extension.op_data_);
    rai::ErrorCode error_code = send->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSend_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    rai::AccountTokenInfo account_token_info;
    bool error = ledger_.AccountTokenInfoGet(
        transaction, block->Account(), send->token_.chain_,
        send->token_.address_, account_token_info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_BALANCE_IS_EMPTY);
    }
    if (account_token_info.balance_.IsZero())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_BALANCE_IS_ZERO);
    }

    rai::TokenKey key(send->token_.chain_, send->token_.address_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    rai::TokenInfo info;
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSend_: get token info failed, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    if (send->token_.type_ != info.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }

    if (!info.circulable_ && rai::IsRaicoin(key.chain_))
    {
        if (block->Account() != key.address_ && send->to_ != key.address_)
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_UNCIRCULABLE, keys);
        }
    }

    rai::TokenValue balance(account_token_info.balance_);
    if (info.type_ == rai::TokenType::_20)
    {
        if (send->value_ > account_token_info.balance_)
        {
            return UpdateLedgerCommon_(
                transaction, block,
                rai::ErrorCode::TOKEN_SEND_MORE_THAN_BALANCE, keys);
        }
        balance = account_token_info.balance_ - send->value_;

        rai::TokenReceivableKey receivable_key(
            send->to_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(
            block->Account(), send->value_, block->Height(),
            rai::TokenType::_20, rai::TokenSource::SEND);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else if (info.type_ == rai::TokenType::_721)
    {
        balance = account_token_info.balance_ - 1;

        rai::TokenValue token_id = send->value_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        if (!ledger_.AccountTokenIdExist(transaction, account_token_id))
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_NOT_OWNED, keys);
        }

        error = ledger_.AccountTokenIdDel(transaction, account_token_id);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessSend_: del account token id, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }

        error_code = UpdateLedgerTokenIdTransfer_(
            transaction, key, token_id, block->Account(), block->Height());
        IF_NOT_SUCCESS_RETURN(error_code);

        rai::TokenReceivableKey receivable_key(
            send->to_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(block->Account(), send->value_,
                                        block->Height(), rai::TokenType::_721,
                                        rai::TokenSource::SEND);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSend_: unexpected token type, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }
    
    error_code =
        UpdateLedgerTokenTransfer_(transaction, key, block->Timestamp(),
                                   block->Account(), block->Height());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerBalance_(transaction, block->Account(), key,
                             account_token_info.balance_, balance);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessReceive_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    auto receive = std::static_pointer_cast<rai::ExtensionTokenReceive>(
        extension.op_data_);
    rai::ErrorCode error_code = receive->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: check data failed, error_code=",
            error_code, ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    // wait source block
    rai::Chain chain = rai::Chain::INVALID;
    if (receive->source_ == rai::TokenSource::SEND
        || receive->source_ == rai::TokenSource::MINT
        || receive->source_ == rai::TokenSource::SWAP)
    {
        std::shared_ptr<rai::Block> source_block(nullptr);
        error_code =
            WaitBlock(transaction, receive->from_, receive->block_height_,
                      block, true, source_block);
        IF_NOT_SUCCESS_RETURN(error_code);
        chain = rai::CurrentChain();
    }
    else if (receive->source_ == rai::TokenSource::MAP)
    {
        // todo: cross chain waiting
        chain = receive->token_.chain_;
        return rai::ErrorCode::APP_PROCESS_WAITING;
    }
    else if (receive->source_ == rai::TokenSource::UNWRAP)
    {
        // todo: cross chain waiting
        chain = receive->unwrap_chain_;
        return rai::ErrorCode::APP_PROCESS_WAITING;
    }
    else
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: unexpected token source, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    rai::TokenKey key(receive->token_.chain_, receive->token_.address_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);

    rai::TokenReceivableKey receivable_key(
        block->Account(), key, chain, receive->tx_hash_);
    rai::TokenReceivable receivable;
    bool error =
        ledger_.TokenReceivableGet(transaction, receivable_key, receivable);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                    rai::ErrorCode::TOKEN_UNRECEIVABLE);
    }

    if (receivable.token_type_ != receive->token_.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                    rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }
    if (receivable.from_ != receive->from_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                    rai::ErrorCode::TOKEN_RECEIVE_FROM, keys);
    }
    if (receivable.block_height_ != receive->block_height_)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_RECEIVE_BLOCK_HEIGHT,
            keys);
    }
    if (receivable.value_ != receive->value_)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_RECEIVE_VALUE, keys);
    }
    if (receivable.source_ != receive->source_)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_RECEIVE_SOURCE, keys);
    }

    rai::TokenInfo info;
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: get token info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    if (info.type_ != receive->token_.type_)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: token type mismatch, tokeninfo.type=",
            info.type_, ", receive.type=", receive->token_.type_,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::TokenValue prev_balance(0);
    rai::AccountTokenInfo account_token_info;
    error =
        ledger_.AccountTokenInfoGet(transaction, block->Account(), key.chain_,
                                    key.address_, account_token_info);
    if (!error)
    {
        prev_balance = account_token_info.balance_;
    }

    rai::TokenValue balance(0);
    if (receivable.token_type_ == rai::TokenType::_20)
    {
        balance = prev_balance + receivable.value_;
        if (balance < prev_balance)
        {
            return UpdateLedgerCommon_(transaction, block,
                                       rai::ErrorCode::TOKEN_BALANCE_OVERFLOW,
                                       keys);
        }
    }
    else if (receivable.token_type_ == rai::TokenType::_721)
    {
        balance = prev_balance + 1;
        if (balance < prev_balance)
        {
            return UpdateLedgerCommon_(transaction, block,
                                       rai::ErrorCode::TOKEN_BALANCE_OVERFLOW,
                                       keys);
        }

        rai::TokenValue token_id = receive->value_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        error = ledger_.AccountTokenIdPut(transaction, account_token_id,
                                          block->Height());
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessSend_: put account token id, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        error_code = UpdateLedgerTokenIdTransfer_(
            transaction, key, token_id, block->Account(), block->Height());
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: unexpected token type, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error = ledger_.TokenReceivableDel(transaction, receivable_key);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessReceive_: delete token receivable record, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
    }

    error_code =
        UpdateLedgerTokenTransfer_(transaction, key, block->Timestamp(),
                                   block->Account(), block->Height());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = UpdateLedgerBalance_(transaction, block->Account(), key,
                                      prev_balance, balance);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSwap_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    auto swap =
        std::static_pointer_cast<rai::ExtensionTokenSwap>(extension.op_data_);
    rai::ErrorCode error_code = swap->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwap_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    using SubOp = rai::ExtensionTokenSwap::SubOp;
    switch (swap->sub_op_)
    {
        case SubOp::CONFIG:
        {
            return ProcessSwapConfig_(transaction, block, *swap);
        }
        case SubOp::MAKE:
        {
            return ProcessSwapMake_(transaction, block, *swap);
        }
        case SubOp::INQUIRY:
        {
            return ProcessSwapInquiry_(transaction, block, *swap);
        }
        case SubOp::INQUIRY_ACK:
        {
            return ProcessSwapInquiryAck_(transaction, block, *swap);
        }
        case SubOp::TAKE:
        {
            return ProcessSwapTake_(transaction, block, *swap);
        }
        case SubOp::TAKE_ACK:
        {
            return ProcessSwapTakeAck_(transaction, block, *swap);
        }
        case SubOp::TAKE_NACK:
        {
            return ProcessSwapTakeNack_(transaction, block, *swap);
        }
        case SubOp::CANCEL:
        {
            return ProcessSwapCancel_(transaction, block, *swap);
        }
        case SubOp::PING:
        {
            return ProcessSwapPing_(transaction, block, *swap);
        }
        case SubOp::PONG:
        {
            return ProcessSwapPong_(transaction, block, *swap);
        }
        default:
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessSwap_: unknown token swap sub-operation, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }
    }

    return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
}

rai::TokenError rai::Token::ProcessSwapConfig_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto config =
        std::static_pointer_cast<rai::ExtensionTokenSwapConfig>(swap.sub_data_);
    if (block->Account() == config->main_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_MAIN_ACCOUNT);
    }

    bool error = ledger_.SwapMainAccountPut(transaction, block->Account(),
                                            config->main_);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapConfig_: put swap main account, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }
    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSwapMake_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto make =
        std::static_pointer_cast<rai::ExtensionTokenSwapMake>(swap.sub_data_);

    rai::AccountTokenInfo account_token_info;
    bool error = ledger_.AccountTokenInfoGet(
        transaction, block->Account(), make->token_offer_.chain_,
        make->token_offer_.address_, account_token_info);
    if (error)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_NO_ENOUGH_BALANCE);
    }

    rai::TokenInfo info;
    rai::TokenKey key(make->token_offer_.chain_, make->token_offer_.address_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapMake_: get token info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::Account main_account;
    error =
        ledger_.SwapMainAccountGet(transaction, block->Account(), main_account);
    if (error || main_account.IsZero())
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_MAIN_ACCOUNT_EMPTY,
            keys);
    }

    rai::AccountSwapInfo account_swap_info;
    error = ledger_.AccountSwapInfoGet(transaction, block->Account(),
                                       account_swap_info);
    if (error)
    {
        account_swap_info = rai::AccountSwapInfo();
    }

    if (account_swap_info.active_orders_
        >= static_cast<uint32_t>(block->Credit()) * rai::ORDERS_PER_CREDIT)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_ORDERS_EXCEEDED,
                                   keys);
    }

    rai::TokenType offer_type = make->token_offer_.type_;
    if (offer_type != info.type_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }

    const rai::ExtensionTokenInfo& offer = make->token_offer_;
    const rai::ExtensionTokenInfo& want = make->token_want_;
    rai::OrderInfo order_info;
    rai::OrderIndex order_index;
    if (offer_type == rai::TokenType::_20)
    {
        if (make->FungiblePair())
        {
            if (make->max_offer_ > account_token_info.balance_)
            {
                return UpdateLedgerCommon_(
                    transaction, block,
                    rai::ErrorCode::TOKEN_SWAP_NO_ENOUGH_BALANCE, keys);
            }
            order_info = rai::OrderInfo(
                main_account, offer.chain_, offer.address_, offer.type_,
                want.chain_, want.address_, want.type_, make->value_offer_,
                make->value_want_, make->min_offer_, make->max_offer_,
                make->timeout_);
            rai::SwapRate rate(make->value_offer_, make->value_want_);
            order_index = rai::OrderIndex(offer.chain_, offer.address_,
                                          want.chain_, want.address_, rate,
                                          block->Account(), block->Height());
        }
        else
        {
            if (make->value_offer_ > account_token_info.balance_)
            {
                return UpdateLedgerCommon_(
                    transaction, block,
                    rai::ErrorCode::TOKEN_SWAP_NO_ENOUGH_BALANCE, keys);
            }
            order_info = rai::OrderInfo(
                main_account, offer.chain_, offer.address_, offer.type_,
                want.chain_, want.address_, want.type_, make->value_offer_,
                make->value_want_, make->timeout_);
            rai::SwapRate rate(make->value_offer_, 1);
            order_index = rai::OrderIndex(
                offer.chain_, offer.address_, want.chain_, want.address_,
                make->value_want_, rate, block->Account(), block->Height());
        }
    }
    else if (offer_type == rai::TokenType::_721)
    {
        if (account_token_info.balance_.IsZero())
        {
            return UpdateLedgerCommon_(
                transaction, block,
                rai::ErrorCode::TOKEN_SWAP_NO_ENOUGH_BALANCE, keys);
        }

        rai::TokenValue token_id = make->value_offer_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        if (!ledger_.AccountTokenIdExist(transaction, account_token_id))
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_NOT_OWNED, keys);
        }

        order_info = rai::OrderInfo(main_account, offer.chain_, offer.address_,
                                    offer.type_, want.chain_, want.address_,
                                    want.type_, make->value_offer_,
                                    make->value_want_, make->timeout_);
        if (make->token_want_.type_ == rai::TokenType::_721)
        {
            rai::SwapRate rate(1, 1);
            order_index = rai::OrderIndex(
                offer.chain_, offer.address_, make->value_offer_, want.chain_,
                want.address_, make->value_want_, rate, block->Account(),
                block->Height());
        }
        else
        {
            rai::SwapRate rate(1, make->value_want_);
            order_index = rai::OrderIndex(
                offer.chain_, offer.address_, make->value_offer_, want.chain_,
                want.address_, rate, block->Account(), block->Height());
        }
    }
    else
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapMake_: unexpected offer token type, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error = ledger_.OrderInfoPut(transaction, block->Account(), block->Height(),
                                 order_info);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSwapMake_: put order info, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    error = ledger_.OrderIndexPut(transaction, order_index);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSwapMake_: put order index, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    rai::ErrorCode error_code =
        UpdateLedgerAccountOrdersInc_(transaction, block->Account());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSwapInquiry_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto inquiry = std::static_pointer_cast<rai::ExtensionTokenSwapInquiry>(
        swap.sub_data_);

    if (inquiry->maker_ == block->Account())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_WITH_SELF);
    }

    std::shared_ptr<rai::Block> order_block(nullptr);
    rai::ErrorCode error_code =
        WaitBlock(transaction, inquiry->maker_, inquiry->order_height_, block,
                  true, order_block);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::OrderInfo order_info;
    bool error = ledger_.OrderInfoGet(transaction, inquiry->maker_,
                                      inquiry->order_height_, order_info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_ORDER_MISS);
    }
    if (order_info.main_ == block->Account())
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_WITH_MAIN_ACCOUNT);
    }

    std::shared_ptr<rai::Block> ack_block(nullptr);
    error_code = WaitBlockIfExist(transaction, order_info.main_,
                                  inquiry->ack_height_, block, true, ack_block);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::AccountTokenInfo account_token_info;
    error = ledger_.AccountTokenInfoGet(
        transaction, block->Account(), order_info.token_want_.chain_,
        order_info.token_want_.address_, account_token_info);
    if (error || account_token_info.balance_.IsZero())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_BALANCE_IS_ZERO);
    }

    rai::TokenInfo info;
    rai::TokenKey key(order_info.token_want_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapInquiry_: get token info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }
    if (info.type_ != order_info.type_want_)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_TYPE_INVALID, keys);
    }

    error = CheckInquiryValue_(order_info, inquiry->value_);
    if (error)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_INQUIRY_VALUE, keys);
    }
    if (info.type_ == rai::TokenType::_20)
    {
        if (inquiry->value_ > account_token_info.balance_)
        {
            return UpdateLedgerCommon_(
                transaction, block,
                rai::ErrorCode::TOKEN_SWAP_NO_ENOUGH_BALANCE, keys);
        }
    }
    else if (info.type_ == rai::TokenType::_721)
    {
        rai::TokenValue token_id = inquiry->value_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        if (!ledger_.AccountTokenIdExist(transaction, account_token_id))
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_NOT_OWNED, keys);
        }
    }
    else
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapInquiry_: unexpected token type, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    rai::SwapInfo swap_info(inquiry->maker_, inquiry->order_height_,
                            inquiry->ack_height_, inquiry->timeout_,
                            inquiry->value_, inquiry->share_);
    error = ledger_.SwapInfoPut(transaction, block->Account(), block->Height(),
                                swap_info);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSwapInquiry_: put swap info, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    rai::InquiryWaiting waiting(order_info.main_, inquiry->ack_height_,
                                block->Account(), block->Height());
    error = ledger_.InquiryWaitingPut(transaction, waiting);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapInquiry_: put inquiry waiting, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (ack_block != nullptr)
    {
        error_code = PurgeInquiryWaiting_(transaction, order_info.main_,
                                          inquiry->ack_height_);
        IF_NOT_SUCCESS_RETURN(error_code);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSwapInquiryAck_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto inquiry_ack =
        std::static_pointer_cast<rai::ExtensionTokenSwapInquiryAck>(
            swap.sub_data_);
    if (inquiry_ack->taker_ == block->Account())
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_TAKER);
    }

    std::shared_ptr<rai::Block> inquiry_block(nullptr);
    rai::ErrorCode error_code =
        WaitBlock(transaction, inquiry_ack->taker_,
                  inquiry_ack->inquiry_height_, block, true, inquiry_block);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::InquiryWaiting waiting(block->Account(), block->Height(),
                                inquiry_ack->taker_,
                                inquiry_ack->inquiry_height_);
    if (!ledger_.InquiryWaitingExist(transaction, waiting))
    {
        return UpdateLedgerCommon_(
            transaction, block,
            rai::ErrorCode::TOKEN_SWAP_INQUIRY_WAITING_MISS);
    }

    rai::SwapInfo swap_info;
    bool error = ledger_.SwapInfoGet(transaction, inquiry_ack->taker_,
                                     inquiry_ack->inquiry_height_, swap_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapInquiryAck_: get swap info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::OrderInfo order_info;
    error = ledger_.OrderInfoGet(transaction, swap_info.maker_,
                                 swap_info.order_height_, order_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapInquiryAck_: get order info failed, hash=",
            block->Hash().StringHex(),
            ", maker=", swap_info.maker_.StringAccount(),
            ", height=", swap_info.order_height_));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::TokenInfo info;
    rai::TokenKey key(order_info.token_offer_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapInquiryAck_: get token info failed, hash=",
            block->Hash().StringHex(), ", chain=", key.chain_,
            ", address=", key.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    if (swap_info.status_ != rai::SwapInfo::Status::INQUIRY)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapInquiryAck_: invalid swap status, hash=",
            block->Hash().StringHex(),
            ", taker=", inquiry_ack->taker_.StringAccount(),
            ", inquiry_height=", inquiry_ack->inquiry_height_));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    std::shared_ptr<rai::Block> previous_block(nullptr);
    error_code =
        WaitBlock(transaction, swap_info.maker_, inquiry_ack->trade_height_ - 1,
                  block, true, previous_block);
    IF_NOT_SUCCESS_RETURN(error_code);
    if (previous_block->Timestamp() > swap_info.timeout_)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_TIMEOUT, keys);
    }

    swap_info.status_ = rai::SwapInfo::Status::INQUIRY_ACK;
    swap_info.maker_ = inquiry_ack->share_;
    swap_info.maker_signature_ = inquiry_ack->signature_;
    swap_info.trade_previous_ = previous_block->Hash();
    swap_info.trade_height_ = inquiry_ack->trade_height_;
    error = ledger_.SwapInfoPut(transaction, inquiry_ack->taker_,
                                inquiry_ack->inquiry_height_, swap_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapInquiryAck_: put swap info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    error_code = UpdateLedgerAccountSwapsInc_(transaction, swap_info.maker_);
    IF_NOT_SUCCESS_RETURN(error_code);
    error_code = UpdateLedgerAccountSwapsInc_(transaction, inquiry_ack->taker_);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSwapTake_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto take =
        std::static_pointer_cast<rai::ExtensionTokenSwapTake>(swap.sub_data_);
    if (take->inquiry_height_ >= block->Height())
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_INQUIRY_HEIGHT);
    }

    rai::SwapInfo swap_info;
    bool error = ledger_.SwapInfoGet(transaction, block->Account(),
                                     take->inquiry_height_, swap_info);
    if (error)
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_MISS);
    }

    rai::OrderInfo order_info;
    error = ledger_.OrderInfoGet(transaction, swap_info.maker_,
                                 swap_info.order_height_, order_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTake_: get order info failed, hash=",
            block->Hash().StringHex(),
            ", maker=", swap_info.maker_.StringAccount(),
            ", height=", swap_info.order_height_));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::TokenInfo info;
    rai::TokenKey key(order_info.token_want_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTake_: get token info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }
    if (info.type_ != order_info.type_want_)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTake_: unexpected token type mismatch, hash=",
            block->Hash().StringHex(), ", expected type=", info.type_,
            ", actual type=", order_info.type_want_));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    std::shared_ptr<rai::Block> inquiry_ack_block(nullptr);
    rai::ErrorCode error_code =
        WaitBlock(transaction, order_info.main_, swap_info.inquiry_ack_height_,
                  block, true, inquiry_ack_block);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (swap_info.status_ != rai::SwapInfo::Status::INQUIRY_ACK)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_STATUS_UNEXPECTED,
                                   keys);
    }

    std::shared_ptr<rai::Block> take_ack_block(nullptr);
    error_code =
        WaitBlockIfExist(transaction, swap_info.maker_, swap_info.trade_height_,
                         block, true, take_ack_block);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::AccountTokenInfo account_token_info;
    error =
        ledger_.AccountTokenInfoGet(transaction, block->Account(), key.chain_,
                                    key.address_, account_token_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTake_: get account token info failed, hash=",
            block->Hash().StringHex(), ", token.chain=", key.chain_,
            ", token.address=", key.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (account_token_info.balance_.IsZero())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_BALANCE_IS_ZERO, keys);
    }

    rai::TokenValue balance;
    if (order_info.type_want_ == rai::TokenType::_20)
    {
        if (swap_info.value_ > account_token_info.balance_)
        {
            return UpdateLedgerCommon_(
                transaction, block,
                rai::ErrorCode::TOKEN_SWAP_NO_ENOUGH_BALANCE, keys);
        }
        balance = account_token_info.balance_ - swap_info.value_;
    }
    else if (order_info.type_want_ == rai::TokenType::_721)
    {
        balance = account_token_info.balance_ - 1;
        rai::TokenValue token_id = swap_info.value_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        if (!ledger_.AccountTokenIdExist(transaction, account_token_id))
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_NOT_OWNED, keys);
        }

        error = ledger_.AccountTokenIdDel(transaction, account_token_id);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessSwapTake_: del account token id, hash=",
                block->Hash().StringHex(), ", token.chain=", key.chain_,
                ", token.address=", key.address_.StringHex(),
                ", token.id=", token_id.StringDec()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }

        error_code = UpdateLedgerTokenIdTransfer_(
            transaction, key, token_id, block->Account(), block->Height());
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTake_: unexpected token type=",
            order_info.type_want_, ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    swap_info.status_ = rai::SwapInfo::Status::TAKE;
    swap_info.take_height_ = block->Height();
    error = ledger_.SwapInfoPut(transaction, block->Account(),
                                take->inquiry_height_, swap_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTake_: put swap info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    rai::TakeWaiting waiting(swap_info.maker_, swap_info.trade_height_,
                             block->Account(), take->inquiry_height_);
    error = ledger_.TakeWaitingPut(transaction, waiting);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTake_: put take waiting failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    error_code =
        UpdateLedgerTokenTransfer_(transaction, key, block->Timestamp(),
                                   block->Account(), block->Height());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerBalance_(transaction, block->Account(), key,
                             account_token_info.balance_, balance);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (take_ack_block != nullptr)
    {
        error_code = PurgeTakeWaiting_(transaction, take_ack_block);
        IF_NOT_SUCCESS_RETURN(error_code);
    }

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSwapTakeAck_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto take_ack = std::static_pointer_cast<rai::ExtensionTokenSwapTakeAck>(
        swap.sub_data_);
    if (take_ack->taker_ == block->Account())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_TAKER);
    }

    std::shared_ptr<rai::Block> take_block(nullptr);
    rai::ErrorCode error_code =
        WaitBlock(transaction, take_ack->taker_, take_ack->take_height_, block,
                  true, take_block);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::TakeWaiting waiting(block->Account(), block->Height(),
                             take_ack->taker_, take_ack->inquiry_height_);
    if (!ledger_.TakeWaitingExist(transaction, waiting))
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_TAKE_WAITING_MISS);
    }

    rai::SwapInfo swap_info;
    bool error = ledger_.SwapInfoGet(transaction, take_ack->taker_,
                                     take_ack->inquiry_height_, swap_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: get swap info failed, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::OrderInfo order_info;
    error = ledger_.OrderInfoGet(transaction, swap_info.maker_,
                                 swap_info.order_height_, order_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: get order info failed, hash=",
            block->Hash().StringHex(),
            ", maker=", swap_info.maker_.StringAccount(),
            ", height=", swap_info.order_height_));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::TokenInfo info;
    rai::TokenKey key(order_info.token_offer_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: get token info failed, hash=",
            block->Hash().StringHex(), ", chain=", key.chain_,
            ", address=", key.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }
    if (info.type_ != order_info.type_offer_)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: unexpected token type mismatch, hash=",
            block->Hash().StringHex(), ", expected type=", info.type_,
            ", actual type=", order_info.type_offer_));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (swap_info.status_ != rai::SwapInfo::Status::TAKE)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: invalid swap status, hash=",
            block->Hash().StringHex(),
            ", taker=", take_ack->taker_.StringAccount(),
            ", inquiry_height=", take_ack->inquiry_height_));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (swap_info.trade_height_ != block->Height())
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: trade height mismatch, hash=",
            block->Hash().StringHex(), ", expected height=",
            swap_info.trade_height_, ", actual height=", block->Height()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (swap_info.maker_ != block->Account())
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSwapTakeAck_: maker mismatch, hash=",
                          block->Hash().StringHex(),
                          ", expected maker=", swap_info.maker_.StringAccount(),
                          ", actual maker=", block->Account().StringAccount()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    if (order_info.Finished())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_ORDER_FINISHED,
                                   keys);
    }

    rai::AccountTokenInfo account_token_info;
    error =
        ledger_.AccountTokenInfoGet(transaction, block->Account(), key.chain_,
                                    key.address_, account_token_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: get account token info failed, hash=",
            block->Hash().StringHex(), ", token.chain=", key.chain_,
            ", token.address=", key.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }
    if (account_token_info.balance_.IsZero())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_BALANCE_IS_ZERO, keys);
    }

    error = CheckTakeAckValue_(order_info, swap_info.value_, take_ack->value_);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_TAKE_ACK_VALUE,
                                   keys);
    }

    rai::TokenValue balance;
    if (info.type_ == rai::TokenType::_20)
    {
        if (take_ack->value_ > account_token_info.balance_)
        {
            return UpdateLedgerCommon_(
                transaction, block,
                rai::ErrorCode::TOKEN_SWAP_NO_ENOUGH_BALANCE, keys);
        }
        balance = account_token_info.balance_ - take_ack->value_;
    }
    else if (info.type_ == rai::TokenType::_721)
    {
        balance = account_token_info.balance_ - 1;
        rai::TokenValue token_id = take_ack->value_;
        rai::AccountTokenId account_token_id(block->Account(), key, token_id);
        if (!ledger_.AccountTokenIdExist(transaction, account_token_id))
        {
            return UpdateLedgerCommon_(
                transaction, block, rai::ErrorCode::TOKEN_ID_NOT_OWNED, keys);
        }

        error = ledger_.AccountTokenIdDel(transaction, account_token_id);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessSwapTakeAck_: del account token id, hash=",
                block->Hash().StringHex(), ", token.chain=", key.chain_,
                ", token.address=", key.address_.StringHex(),
                ", token.id=", token_id.StringDec()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }

        error_code = UpdateLedgerTokenIdTransfer_(
            transaction, key, token_id, block->Account(), block->Height());
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    else
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: unexpected token type=", info.type_,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    error_code =
        UpdateLedgerTokenTransfer_(transaction, key, block->Timestamp(),
                                   block->Account(), block->Height());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerBalance_(transaction, block->Account(), key,
                             account_token_info.balance_, balance);
    IF_NOT_SUCCESS_RETURN(error_code);

    // put receivable
    {
        rai::TokenReceivableKey receivable_key(
            take_ack->taker_, key, rai::CurrentChain(), block->Hash());
        rai::TokenReceivable receivable(swap_info.maker_, take_ack->value_,
                                        block->Height(), info.type_,
                                        rai::TokenSource::SWAP);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }
    {
        rai::TokenReceivableKey receivable_key(
            swap_info.maker_, order_info.token_want_, rai::CurrentChain(),
            block->Hash());
        rai::TokenReceivable receivable(swap_info.maker_, swap_info.value_,
                                        block->Height(), order_info.type_want_,
                                        rai::TokenSource::SWAP);
        error_code =
            UpdateLedgerReceivable_(transaction, receivable_key, receivable);
        IF_NOT_SUCCESS_RETURN(error_code);
    }

    error = ledger_.TakeWaitingDel(transaction, waiting);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: delete waiting record failed, "
            "maker=",
            waiting.maker_.StringAccount(), ", trade_height=",
            waiting.trade_height_, ", taker=", waiting.taker_.StringAccount(),
            ", inquiry_height=", waiting.inquiry_height_));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    swap_info.status_ = rai::SwapInfo::Status::TAKE_ACK;
    error = ledger_.SwapInfoPut(transaction, take_ack->taker_,
                                take_ack->inquiry_height_, swap_info);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSwapTakeAck_: put swap info, account=",
                          take_ack->taker_.StringAccount(),
                          ", height=", take_ack->inquiry_height_));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    if (order_info.type_offer_ == rai::TokenType::_20
        && order_info.type_want_ == rai::TokenType::_20)
    {
        // underflow checked in CheckTakeAckValue_
        order_info.left_ -= take_ack->value_;
        if (order_info.left_.IsZero())
        {
            order_info.finished_height_ = block->Height();
            order_info.finished_by_ = rai::OrderInfo::FinishedBy::FULFILL;
        }
    }
    else
    {
        order_info.finished_height_ = block->Height();
        order_info.finished_by_ = rai::OrderInfo::FinishedBy::FULFILL;
    }
    error = ledger_.OrderInfoPut(transaction, swap_info.maker_,
                                 swap_info.order_height_, order_info);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSwapTakeAck_: put order info, maker=",
                          swap_info.maker_.StringAccount(),
                          ", height=", swap_info.order_height_));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    if (order_info.Finished())
    {
        rai::OrderIndex order_index = MakeOrderIndex_(
            order_info, swap_info.maker_, swap_info.order_height_);
        error = ledger_.OrderIndexDel(transaction, order_index);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::ProcessSwapTakeAck_: delete order index, maker=",
                swap_info.maker_.StringAccount(),
                ", height=", swap_info.order_height_));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }

        error_code =
            UpdateLedgerAccountOrdersDec_(transaction, block->Account());
        IF_NOT_SUCCESS_RETURN(error_code);
    }

    error_code = UpdateLedgerSwapIndex_(
        transaction, order_info, swap_info, take_ack->taker_,
        take_ack->inquiry_height_, block->Timestamp());
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code = UpdateLedgerAccountSwapsDec_(transaction, swap_info.maker_);
    IF_NOT_SUCCESS_RETURN(error_code);
    error_code = UpdateLedgerAccountSwapsDec_(transaction, take_ack->taker_);
    IF_NOT_SUCCESS_RETURN(error_code);

    error_code =
        UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS, keys);
    IF_NOT_SUCCESS_RETURN(error_code);

    return rai::ErrorCode::SUCCESS;
}

rai::TokenError rai::Token::ProcessSwapTakeNack_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto take_nack = std::static_pointer_cast<rai::ExtensionTokenSwapTakeNack>(
        swap.sub_data_);
    if (take_nack->taker_ == block->Account())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_TAKER);
    }

    std::shared_ptr<rai::Block> inquiry_block(nullptr);
    rai::ErrorCode error_code =
        WaitBlock(transaction, take_nack->taker_, take_nack->inquiry_height_,
                  block, true, inquiry_block);
    IF_NOT_SUCCESS_RETURN(error_code);

    rai::SwapInfo swap_info;
    bool error = ledger_.SwapInfoGet(transaction, take_nack->taker_,
                                     take_nack->inquiry_height_, swap_info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_MISS);
    }

    if (swap_info.maker_ != block->Account())
    {
        return UpdateLedgerCommon_(
            transaction, block, rai::ErrorCode::TOKEN_SWAP_ACCOUNT_NOT_MAKER);
    }

    rai::OrderInfo order_info;
    error = ledger_.OrderInfoGet(transaction, swap_info.maker_,
                                 swap_info.order_height_, order_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeNack_: get order info failed, hash=",
            block->Hash().StringHex(),
            ", maker=", swap_info.maker_.StringAccount(),
            ", height=", swap_info.order_height_));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }

    rai::TokenInfo info;
    rai::TokenKey key(order_info.token_offer_);
    std::vector<rai::TokenKey> keys;
    keys.push_back(key);
    error = ledger_.TokenInfoGet(transaction, key, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeNack_: get token info failed, hash=",
            block->Hash().StringHex(), ", chain=", key.chain_,
            ", address=", key.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
    }
    if (info.type_ != order_info.type_offer_)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeNack_: unexpected token type mismatch, hash=",
            block->Hash().StringHex(), ", expected type=", info.type_,
            ", actual type=", order_info.type_offer_));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    std::shared_ptr<rai::Block> inquiry_ack_block(nullptr);
    error_code =
        WaitBlock(transaction, order_info.main_, swap_info.inquiry_ack_height_,
                  block, true, inquiry_ack_block);
    IF_NOT_SUCCESS_RETURN(error_code);

    if (swap_info.status_ != rai::SwapInfo::Status::INQUIRY_ACK
        && swap_info.status_ != rai::SwapInfo::Status::TAKE)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_STATUS_UNEXPECTED,
                                   keys);
    }

    if (swap_info.trade_height_ != block->Height())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_TAKE_NACK_HEIGHT,
                                   keys);
    }

    return UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS,
                               keys);
}

rai::TokenError rai::Token::ProcessSwapCancel_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto cancel = std::static_pointer_cast<rai::ExtensionTokenSwapCancel>(
        swap.sub_data_);

    rai::Account maker = block->Account();
    uint64_t order_height = cancel->order_height_;
    rai::OrderInfo order_info;
    bool error =
        ledger_.OrderInfoGet(transaction, maker, order_height, order_info);
    if (error)
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_ORDER_MISS);
    }

    std::vector<rai::TokenKey> keys;
    keys.push_back(rai::TokenKey(order_info.token_offer_));
    if (order_info.Finished())
    {
        return UpdateLedgerCommon_(transaction, block,
                                   rai::ErrorCode::TOKEN_SWAP_ORDER_FINISHED,
                                   keys);
    }

    order_info.finished_by_ = rai::OrderInfo::FinishedBy::CANCEL;
    order_info.finished_height_ = block->Height();
    error = ledger_.OrderInfoPut(transaction, maker, order_height, order_info);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::ProcessSwapCancel_: put order info, maker=",
                          maker.StringAccount(), ", height=", order_height));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    rai::OrderIndex order_index =
        MakeOrderIndex_(order_info, maker, order_height);
    error = ledger_.OrderIndexDel(transaction, order_index);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessSwapTakeAck_: delete order index, maker=",
            maker.StringAccount(), ", height=", order_height));
        return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
    }

    rai::ErrorCode error_code =
        UpdateLedgerAccountOrdersDec_(transaction, block->Account());
    IF_NOT_SUCCESS_RETURN(error_code);

    return UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS,
                               keys);
}

rai::TokenError rai::Token::ProcessSwapPing_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    auto ping =
        std::static_pointer_cast<rai::ExtensionTokenSwapPing>(swap.sub_data_);

    return UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS);
}

rai::TokenError rai::Token::ProcessSwapPong_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionTokenSwap& swap)
{
    return UpdateLedgerCommon_(transaction, block, rai::ErrorCode::SUCCESS);
}

rai::TokenError rai::Token::ProcessUnmap_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    auto unmap = std::static_pointer_cast<rai::ExtensionTokenUnmap>(
        extension.op_data_);
    rai::ErrorCode error_code = unmap->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessUnmap_: check data failed, error_code=",
            error_code, ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    // todo: cross chain
    return rai::ErrorCode::APP_PROCESS_WAITING;
}

rai::TokenError rai::Token::ProcessWrap_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    const rai::ExtensionToken& extension)
{
    auto wrap =
        std::static_pointer_cast<rai::ExtensionTokenWrap>(extension.op_data_);
    rai::ErrorCode error_code = wrap->CheckData();
    if (error_code != rai::ErrorCode::SUCCESS)
    {
        rai::Log::Error(rai::ToString(
            "Token::ProcessWrap_: check data failed, error_code=", error_code,
            ", hash=", block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    // todo: cross chain
    return rai::ErrorCode::APP_PROCESS_WAITING;
}

rai::ErrorCode rai::Token::ProcessError_(rai::Transaction& transaction,
                                         const rai::TokenError& token_error)
{
    return token_error.return_code_;
}

rai::ErrorCode rai::Token::PurgeInquiryWaiting_(
    rai::Transaction& transaction, const rai::Account& main_account,
    uint64_t height)
{
    std::vector<rai::AccountHeight> inquiries;
    rai::Iterator i =
        ledger_.InquiryWaitingLowerBound(transaction, main_account, height);
    rai::Iterator n =
        ledger_.InquiryWaitingUpperBound(transaction, main_account, height);
    for (; i != n; ++i)
    {
        rai::InquiryWaiting waiting;
        bool error = ledger_.InquiryWaitingGet(i, waiting);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeInquiryWaiting_: get inquiry waiting record, "
                "account=",
                main_account.StringAccount(), ", height=", height));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        if (main_account != waiting.main_ || height != waiting.ack_height_)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeInquiryWaiting_: invalid record, "
                "expected_account=",
                main_account.StringAccount(), ", expected_height=", height,
                ", actual_account=", waiting.main_.StringAccount(),
                ", actual_height=", waiting.ack_height_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }
        inquiries.push_back(
            rai::AccountHeight{waiting.taker_, waiting.inquiry_height_});
    }

    for (const auto& i : inquiries)
    {
        rai::SwapInfo info;
        bool error =
            ledger_.SwapInfoGet(transaction, i.account_, i.height_, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeInquiryWaiting_: get swap info, account=",
                i.account_.StringAccount(), ", height=", i.height_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        if (info.status_ != rai::SwapInfo::Status::INQUIRY)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeInquiryWaiting_: unexpected swap status, account=",
                i.account_.StringAccount(), ", height=", i.height_,
                ", status=", info.status_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        info.status_ = rai::SwapInfo::Status::INQUIRY_NACK;
        error = ledger_.SwapInfoPut(transaction, i.account_, i.height_, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeInquiryWaiting_: put swap info, account=",
                i.account_.StringAccount(), ", height=", i.height_));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        rai::InquiryWaiting waiting(main_account, height, i.account_,
                                    i.height_);
        error = ledger_.InquiryWaitingDel(transaction, waiting);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeInquiryWaiting_: delete waiting record failed, "
                "main_account=",
                main_account.StringAccount(), ", ack_height=", height,
                ", taker_account=", i.account_.StringAccount(),
                ", inquiry_height=", i.height_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::PurgeTakeWaiting_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block)
{
    rai::Account maker = block->Account();
    uint64_t height = block->Height();
    std::vector<rai::AccountHeight> takes;
    rai::Iterator i = ledger_.TakeWaitingLowerBound(transaction, maker, height);
    rai::Iterator n = ledger_.TakeWaitingUpperBound(transaction, maker, height);
    for (; i != n; ++i)
    {
        rai::TakeWaiting waiting;
        bool error = ledger_.TakeWaitingGet(i, waiting);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: get take waiting record, "
                "account=",
                maker.StringAccount(), ", height=", height));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        if (maker != waiting.maker_ || height != waiting.trade_height_)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: invalid record, "
                "expected_account=",
                maker.StringAccount(), ", expected_height=", height,
                ", actual_account=", waiting.maker_.StringAccount(),
                ", actual_height=", waiting.trade_height_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        takes.push_back(
            rai::AccountHeight{waiting.taker_, waiting.inquiry_height_});
    }

    rai::ErrorCode error_code = rai::ErrorCode::SUCCESS;
    for (const auto& i : takes)
    {
        rai::SwapInfo swap_info;
        bool error =
            ledger_.SwapInfoGet(transaction, i.account_, i.height_, swap_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: get swap info, account=",
                i.account_.StringAccount(), ", height=", i.height_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        if (swap_info.status_ != rai::SwapInfo::Status::TAKE)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: unexpected swap status, account=",
                i.account_.StringAccount(), ", height=", i.height_,
                ", status=", swap_info.status_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        rai::OrderInfo order_info;
        error = ledger_.OrderInfoGet(transaction, swap_info.maker_,
                                     swap_info.order_height_, order_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: get order info failed, maker=",
                swap_info.maker_.StringAccount(),
                ", height=", swap_info.order_height_));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }

        rai::TokenInfo info;
        rai::TokenKey key(order_info.token_want_);
        error = ledger_.TokenInfoGet(transaction, key, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: get token info failed, tokne.chain=",
                key.chain_, ", address=", key.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        if (info.type_ != order_info.type_want_)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: unexpected token type mismatch, "
                "expected type=",
                info.type_, ", actual type=", order_info.type_want_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        if (info.type_ == rai::TokenType::_20
            || info.type_ == rai::TokenType::_721)
        {
            rai::TokenReceivableKey receivable_key(
                i.account_, key, rai::CurrentChain(), block->Hash());
            rai::TokenReceivable receivable(maker, swap_info.value_, height,
                                            info.type_, rai::TokenSource::SWAP);
            error_code = UpdateLedgerReceivable_(transaction, receivable_key,
                                                 receivable);
            IF_NOT_SUCCESS_RETURN(error_code);
        }
        else
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: unexpected token type=",
                order_info.type_want_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }

        swap_info.status_ = rai::SwapInfo::Status::TAKE_NACK;
        error =
            ledger_.SwapInfoPut(transaction, i.account_, i.height_, swap_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: put swap info, account=",
                i.account_.StringAccount(), ", height=", i.height_));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        rai::TakeWaiting waiting(maker, height, i.account_, i.height_);
        error = ledger_.TakeWaitingDel(transaction, waiting);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::PurgeTakeWaiting_: delete waiting record failed, "
                "maker=",
                maker.StringAccount(), ", trade_height=", height, ", taker=",
                i.account_.StringAccount(), ", inquiry_height=", i.height_));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerCommon_(
    rai::Transaction& transaction, const std::shared_ptr<rai::Block>& block,
    rai::ErrorCode status, const std::vector<rai::TokenKey>& tokens)
{
    rai::Account account = block->Account();
    uint64_t height = block->Height();
    rai::AccountTokensInfo info;
    bool error = ledger_.AccountTokensInfoGet(transaction, account, info);
    if (error)
    {
        info = rai::AccountTokensInfo();
    }
    else
    {
        if (height <= info.head_)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpateLedgerCommon_: unexpected block height, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
        }
    }

    rai::TokenBlock token_block(info.head_, block->Hash(), status);
    error = ledger_.TokenBlockPut(transaction, account, height, token_block);
    if (error)
    {
        rai::Log::Error(
            rai::ToString("Token::UpateLedgerCommon_: put token block, hash=",
                          block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    info.head_ = height;
    info.blocks_ += 1;
    info.last_active_ = block->Timestamp();
    error = ledger_.AccountTokensInfoPut(transaction, account, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpateLedgerCommon_: put account tokens info, hash=",
            block->Hash().StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    for (const auto& token : tokens)
    {
        rai::AccountTokenInfo account_token_info;
        error = ledger_.AccountTokenInfoGet(transaction, account, token.chain_,
                                            token.address_, account_token_info);
        if (error)
        {
            account_token_info = rai::AccountTokenInfo();
        }
        else
        {
            if (height <= account_token_info.head_
                && account_token_info.head_ != rai::Block::INVALID_HEIGHT)
            {
                rai::Log::Error(rai::ToString(
                    "Token::UpateLedgerCommon_: unexpected block height, hash=",
                    block->Hash().StringHex()));
                return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
            }
        }

        rai::AccountTokenLink link(account, token.chain_, token.address_,
                                   height);
        error = ledger_.AccountTokenLinkPut(transaction, link,
                                            account_token_info.head_);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpateLedgerCommon_: put account token link, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        account_token_info.head_ = height;
        account_token_info.blocks_ += 1;
        error = ledger_.AccountTokenInfoPut(transaction, account, token.chain_,
                                            token.address_, account_token_info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpateLedgerCommon_: put account token info, hash=",
                block->Hash().StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }
}

rai::ErrorCode rai::Token::UpdateLedgerReceivable_(
    rai::Transaction& transaction, const rai::TokenReceivableKey& key,
    const rai::TokenReceivable& receivable)
{
    bool error = ledger_.TokenReceivablePut(transaction, key, receivable);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerReceivable_: to=", key.to_.StringAccount(),
            ", token.chain=", key.token_.chain_,
            ", token.address=", key.token_.address_.StringHex(),
            ", tx_hash=", key.tx_hash_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    if (receivable_observer_)
    {
        receivable_observer_(key, receivable);
    }
    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerBalance_(rai::Transaction& transaction,
                                                const rai::Account& account,
                                                const rai::TokenKey& token,
                                                const rai::TokenValue& previous,
                                                const rai::TokenValue& current)
{
    if (previous == current)
    {
        return rai::ErrorCode::SUCCESS;
    }

    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerBalance_: get token info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    bool updated = false;
    if (previous == 0)
    {
        info.holders_ += 1;
        updated = true;
    }

    if (current == 0)
    {
        if (info.holders_ == 0)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: token holders inconsistency, "
                "token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        info.holders_ -= 1;
        updated = true;
    }

    if (updated)
    {
        error = ledger_.TokenInfoPut(transaction, token, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: put token info failed, "
                "token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }

    if (previous > 0)
    {
        if (!ledger_.TokenHolderExist(transaction, token, account, previous))
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: token holder doesn't exist, "
                "token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;  // Ledger inconsistency
        }
        error = ledger_.TokenHolderDel(transaction, token, account, previous);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: del token holder, account=",
                account.StringAccount(), ", token.chain=", token.chain_,
                ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }
    }

    if (current > 0)
    {
        error = ledger_.TokenHolderPut(transaction, token, account, current);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerBalance_: put token holder, account=",
                account.StringAccount(), ", token.chain=", token.chain_,
                ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_DEL;
        }
    }

    rai::AccountTokenInfo account_token_info;
    error = ledger_.AccountTokenInfoGet(transaction, account, token.chain_,
                                        token.address_, account_token_info);
    if (error)
    {
        account_token_info = rai::AccountTokenInfo();
    }
    account_token_info.balance_ = current;

    error = ledger_.AccountTokenInfoPut(transaction, account, token.chain_,
                                        token.address_, account_token_info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerBalance_: put account token info, account=",
            account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerSupplies_(
    rai::Transaction& transaction, const rai::TokenKey& token,
    const rai::TokenValue& total_supply, const rai::TokenValue& local_supply)
{
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerSupplies_: get token info failed, token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    info.total_supply_ = total_supply;
    info.local_supply_ = local_supply;
    error = ledger_.TokenInfoPut(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerSupplies_: put token info failed, token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerTokenTransfer_(
    rai::Transaction& transaction, const rai::TokenKey& token,
    uint64_t timestamp, const rai::Account& account, uint64_t height)
{
    rai::TokenInfo info;
    bool error = ledger_.TokenInfoGet(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: get token info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    ++info.transfers_;
    error = ledger_.TokenInfoPut(transaction, token, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: put token info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    rai::TokenTransfer transfer(token, timestamp, account, height);
    error = ledger_.TokenTransferPut(transaction, transfer);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: put token transfer failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex(),
            ", account=", account.StringAccount(), ", height=", height));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerTokenIdTransfer_(
    rai::Transaction& transaction, const rai::TokenKey& token,
    const rai::TokenValue& id, const rai::Account& account, uint64_t height)
{
    rai::TokenIdInfo info;
    bool error = ledger_.TokenIdInfoGet(transaction, token, id, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: get token id info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex(),
            ", id=", id.StringDec()));
        return rai::ErrorCode::APP_PROCESS_UNEXPECTED;
    }

    uint64_t index = info.transfers_++;
    error = ledger_.TokenIdInfoPut(transaction, token, id, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: put token id info failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex(),
            ", id=", id.StringDec()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    rai::TokenIdTransferKey key(token, id, index);
    error = ledger_.TokenIdTransferPut(transaction, key, account, height);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerTokenIdTransfer_: put token id transfer failed, "
            "token.chain=",
            token.chain_, ", token.address=", token.address_.StringHex(),
            ", id=", id.StringDec()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerAccountOrdersInc_(
    rai::Transaction& transaction, const rai::Account& account)
{
    rai::AccountSwapInfo info;
    bool error = ledger_.AccountSwapInfoGet(transaction, account, info);
    if (error)
    {
        info = rai::AccountSwapInfo();
    }

    ++info.active_orders_;
    ++info.total_orders_;
    
    error = ledger_.AccountSwapInfoPut(transaction, account, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerAccountOrdersInc_: put account swap info, "
            "account=", account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerAccountOrdersDec_(
    rai::Transaction& transaction, const rai::Account& account)
{
    rai::AccountSwapInfo info;
    bool error = ledger_.AccountSwapInfoGet(transaction, account, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerAccountOrdersDec_: get account swap info, "
            "account=", account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_HALT;
    }

    if (info.active_orders_ == 0)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerAccountOrdersDec_: active orders underflow, "
            "account=",
            account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_HALT;
    }

    --info.active_orders_;
    
    error = ledger_.AccountSwapInfoPut(transaction, account, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerAccountOrdersDec_: put account swap info, "
            "account=", account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerAccountSwapsInc_(
    rai::Transaction& transaction, const rai::Account& account)
{
    rai::AccountSwapInfo info;
    bool error = ledger_.AccountSwapInfoGet(transaction, account, info);
    if (error)
    {
        info = rai::AccountSwapInfo();
    }

    ++info.active_swaps_;
    ++info.total_swaps_;
    
    error = ledger_.AccountSwapInfoPut(transaction, account, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerAccountSwapsInc_: put account swap info, "
            "account=", account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerAccountSwapsDec_(
    rai::Transaction& transaction, const rai::Account& account)
{
    rai::AccountSwapInfo info;
    bool error = ledger_.AccountSwapInfoGet(transaction, account, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerAccountSwapsDec_: get account swap info, "
            "account=",
            account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_HALT;
    }

    if (info.active_swaps_ == 0)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerAccountSwapsDec_: active swaps underflow, "
            "account=",
            account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_HALT;
    }

    --info.active_swaps_;

    error = ledger_.AccountSwapInfoPut(transaction, account, info);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerAccountSwapsDec_: put account swap info, "
            "account=", account.StringAccount()));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    return rai::ErrorCode::SUCCESS;
}

rai::ErrorCode rai::Token::UpdateLedgerSwapIndex_(
    rai::Transaction& transaction, const rai::OrderInfo& order_info,
    const rai::SwapInfo& swap_info, const rai::Account& taker,
    uint64_t inquiry_height, uint64_t timestamp)
{
    // put OrderSwapIndex
    rai::OrderSwapIndex order_swap_index(swap_info.maker_,
                                         swap_info.order_height_, taker,
                                         inquiry_height, timestamp);
    bool error = ledger_.OrderSwapIndexPut(transaction, order_swap_index);
    if (error)
    {
        rai::Log::Error(rai::ToString(
            "Token::UpdateLedgerSwapIndex_: put order swap index, maker=",
            swap_info.maker_.StringAccount(), ", order height=",
            swap_info.order_height_, ", taker=", taker.StringAccount(),
            ", inquiry height=", inquiry_height));
        return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
    }

    std::vector<rai::TokenKey> tokens{order_info.token_offer_,
                                      order_info.token_want_};
    for (const auto& token : tokens)
    {
        rai::TokenSwapIndex index(token, taker, inquiry_height, timestamp);
        error = ledger_.TokenSwapIndexPut(transaction, index);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerSwapIndex_: put token swap index, "
                "token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex(),
                ", taker=", taker.StringAccount(),
                ", inquiry height=", inquiry_height));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }

        rai::TokenInfo info;
        error = ledger_.TokenInfoGet(transaction, token, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerSwapIndex_: get token info, token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_HALT;
        }
        ++info.swaps_;
        error = ledger_.TokenInfoPut(transaction, token, info);
        if (error)
        {
            rai::Log::Error(rai::ToString(
                "Token::UpdateLedgerSwapIndex_: put token info, token.chain=",
                token.chain_, ", token.address=", token.address_.StringHex()));
            return rai::ErrorCode::APP_PROCESS_LEDGER_PUT;
        }
    }

    return rai::ErrorCode::SUCCESS;
}

bool rai::Token::CheckInquiryValue_(const rai::OrderInfo& order,
                                    const rai::TokenValue& value) const
{
    if (order.type_offer_ == rai::TokenType::_20
        && order.type_want_ == rai::TokenType::_20)
    {
        if (value.IsZero()) return true;
        rai::uint512_t offer_s(order.value_offer_.Number());
        rai::uint512_t want_s(order.value_want_.Number());
        rai::uint512_t value_s(value.Number());
        return (offer_s * value_s % want_s) != 0;
    }
    else if (order.type_offer_ == rai::TokenType::_20
             && order.type_want_ == rai::TokenType::_721)
    {
        return order.value_want_ != value;
    }
    else if (order.type_offer_ == rai::TokenType::_721
             && order.type_want_ == rai::TokenType::_20)
    {
        if (value.IsZero()) return true;
        return order.value_want_ != value;
    }
    else if (order.type_offer_ == rai::TokenType::_721
             && order.type_want_ == rai::TokenType::_721)
    {
        return order.value_want_ != value;
    }
    else
    {
        return true;
    }
}

bool rai::Token::CheckTakeAckValue_(const rai::OrderInfo& order,
                                    const rai::TokenValue& take_value,
                                    const rai::TokenValue& ack_value) const
{
    if (order.type_offer_ == rai::TokenType::_20
        && order.type_want_ == rai::TokenType::_20)
    {
        if (ack_value.IsZero()) return true;
        if (ack_value > order.left_) return true;
        if (ack_value < order.min_offer_ && ack_value != order.left_)
            return true;
        rai::uint512_t offer_s(order.value_offer_.Number());
        rai::uint512_t want_s(order.value_want_.Number());
        rai::uint512_t take_s(take_value.Number());
        rai::uint512_t ack_s(ack_value.Number());
        return offer_s * take_s != want_s * ack_s;
    }
    else if (order.type_offer_ == rai::TokenType::_20
             && order.type_want_ == rai::TokenType::_721)
    {
        if (ack_value.IsZero()) return true;
        return ack_value != order.value_offer_;
    }
    else if (order.type_offer_ == rai::TokenType::_721
             && order.type_want_ == rai::TokenType::_20)
    {
        return ack_value != order.value_offer_;
    }
    else if (order.type_offer_ == rai::TokenType::_721
             && order.type_want_ == rai::TokenType::_721)
    {
        return ack_value != order.value_offer_;
    }
    else
    {
        return true;
    }
}

rai::OrderIndex rai::Token::MakeOrderIndex_(const rai::OrderInfo& info,
                                            const rai::Account& maker,
                                            uint64_t height) const
{
    if (info.type_offer_ == rai::TokenType::_20
        && info.type_want_ == rai::TokenType::_20)
    {
        rai::SwapRate rate(info.value_offer_, info.value_want_);
        return rai::OrderIndex(info.token_offer_.chain_,
                               info.token_offer_.address_,
                               info.token_want_.chain_,
                               info.token_want_.address_, rate, maker, height);
    }
    else if (info.type_offer_ == rai::TokenType::_20
             && info.type_want_ == rai::TokenType::_721)
    {
        rai::SwapRate rate(info.value_offer_, 1);
        return rai::OrderIndex(
            info.token_offer_.chain_, info.token_offer_.address_,
            info.token_want_.chain_, info.token_want_.address_,
            info.value_want_, rate, maker, height);
    }
    else if (info.type_offer_ == rai::TokenType::_721
             && info.type_want_ == rai::TokenType::_20)
    {
        rai::SwapRate rate(1, info.value_want_);
        return rai::OrderIndex(info.token_offer_.chain_,
                               info.token_offer_.address_, info.value_offer_,
                               info.token_want_.chain_,
                               info.token_want_.address_, rate, maker, height);
    }
    else if (info.type_offer_ == rai::TokenType::_721
             && info.type_want_ == rai::TokenType::_721)
    {
        rai::SwapRate rate(1, 1);
        return rai::OrderIndex(
            info.token_offer_.chain_, info.token_offer_.address_,
            info.value_offer_, info.token_want_.chain_,
            info.token_want_.address_, info.value_want_, rate, maker, height);
    }
    else
    {
        return rai::OrderIndex();
    }
}
