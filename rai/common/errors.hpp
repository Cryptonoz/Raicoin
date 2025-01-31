#pragma once
#include <string>

namespace rai
{
enum class ErrorCode : int
{
    SUCCESS = 0,

    // common errors: 1 ~ 199
    GENERIC                              = 1,
    STREAM                               = 2,
    BLOCK_TYPE                           = 3,
    BLOCK_OPCODE                         = 4,
    EXTENSIONS_LENGTH                    = 5,
    UNKNOWN_COMMAND                      = 6,
    DATA_PATH                            = 7,
    CONFIG_VERSION                       = 8,
    OPEN_OR_CREATE_FILE                  = 9,
    WRITE_FILE                           = 10,
    CONFIG_NODE_VERSION                  = 11,
    CONFIG_LOG_VERSION                   = 12,
    PASSWORD_LENGTH                      = 13,
    PASSWORD_MATCH                       = 14,
    KEY_FILE_EXIST                       = 15,
    DERIVE_KEY                           = 16,
    CMD_MISS_KEY                         = 17,
    KEY_FILE_NOT_EXIST                   = 18,
    KEY_FILE_VERSION                     = 19,
    INVALID_KEY_FILE                     = 20,
    PASSWORD_ERROR                       = 21,
    MAGIC_NUMBER                         = 22,
    INVALID_MESSAGE                      = 23,
    UNKNOWN_MESSAGE                      = 24,
    HANDSHAKE_TYPE                       = 25,
    OUTDATED_VERSION                     = 26,
    UNKNOWN_VERSION                      = 27,
    SIGNATURE                            = 28,
    KEEPLIVE_PEERS                       = 29,
    KEEPLIVE_REACHABLE_PEERS             = 30,
    CONFIG_RPC_VERSION                   = 31,
    PUBLISH_NEED_CONFIRM                 = 32,
    BLOCK_CREDIT                         = 33,
    BLOCK_COUNTER                        = 34,
    BLOCK_TIMESTAMP                      = 35,
    MDB_ENV_CREATE                       = 36,
    MDB_ENV_SET_MAXDBS                   = 37,
    MDB_ENV_SET_MAPSIZE                  = 38,
    MDB_ENV_OPEN                         = 39,
    MDB_TXN_BEGIN                        = 40,
    MDB_DBI_OPEN                         = 41,
    MESSAGE_QUERY_BY                     = 42,
    MESSAGE_QUERY_STATUS                 = 43,
    MESSAGE_QUERY_BLOCK                  = 44,
    MESSAGE_CONFIRM_SIGNATURE            = 45,
    MESSAGE_FORK_ACCOUNT                 = 46,
    MESSAGE_FORK_HEIGHT                  = 47,
    MESSAGE_FORK_BLOCK_EQUAL             = 48,
    MESSAGE_CONFLICT_BLOCK               = 49,
    MESSAGE_CONFLICT_SIGNATURE           = 50,
    MESSAGE_CONFLICT_TIMESTAMP           = 51,
    ACCOUNT_LIMITED                      = 52,
    LEDGER_ACCOUNT_INFO_GET              = 53,
    LEDGER_BLOCK_GET                     = 54,
    CMD_MISS_HASH                        = 55,
    CMD_INVALID_HASH                     = 56,
    LEDGER_RECEIVABLE_INFO_COUNT         = 57,
    LEDGER_BLOCK_COUNT                   = 58,
    LEDGER_ACCOUNT_COUNT                 = 59,
    CONFIG_WALLET_VERSION                = 60,
    WALLET_PASSWORD                      = 61,
    WALLET_LOCKED                        = 62,
    WALLET_INFO_PUT                      = 63,
    WALLET_ACCOUNT_INFO_PUT              = 64,
    WALLET_GET                           = 65,
    WALLET_ACCOUNT_GET                   = 66,
    WALLET_ACCOUNT_EXISTS                = 67,
    DNS_RESOLVE                          = 68,
    TCP_CONNECT                          = 69,
    HTTP_POST                            = 70,
    SUBSCRIBE_TIMESTAMP                  = 71,
    SUBSCRIBE_SIGNATURE                  = 72,
    SUBSCRIBE_NO_CALLBACK                = 73,
    ACCOUNT_ACTION_TOO_QUICKLY           = 74,
    ACCOUNT_ACTION_CREDIT                = 75,
    ACCOUNT_ACTION_BALANCE               = 76,
    LEDGER_RECEIVABLES_GET               = 77,
    WALLET_NOT_SELECTED_ACCOUNT          = 78,
    LEDGER_RECEIVABLE_INFO_GET           = 79,
    WALLET_RECEIVABLE_LESS_THAN_CREDIT   = 80,
    REWARDER_REWARDABLE_LESS_THAN_CREDIT = 81,
    LEDGER_REWARDABLE_INFO_GET           = 82,
    REWARDER_TIMESTAMP                   = 83,
    REWARDER_AMOUNT                      = 84,
    ACCOUNT_ZERO_BALANCE                 = 85,
    PEER_QUERY                           = 86,
    WALLET_EXISTS                        = 87,
    ACCOUNT_MAX_CREDIT                   = 88,
    INVALID_URL                          = 89,
    HTTP_CLIENT_USED                     = 90,
    LOAD_CERT                            = 91,
    SET_SSL_SNI                          = 92,
    SSL_HANDSHAKE                        = 93,
    WRITE_STREAM                         = 94,
    HTTP_GET                             = 95,
    LOGIC_ERROR                          = 96,
    MESSAGE_CONFIRM_TIMESTAMP            = 97,
    ELECTION_TALLY                       = 98,
    UDP_RECEIVE                          = 99,
    RESERVED_IP                          = 100,
    REWARD_TO_ACCOUNT                    = 101,
    WALLET_ACCOUNT_IN_SYNC               = 102,
    EXTENSION_APPEND                     = 103,
    ACCOUNT_RESTRICTED                   = 104,
    LEDGER_SOURCE_PUT                    = 105,
    LEDGER_VERSION_PUT                   = 106,
    LEDGER_UNKNOWN_VERSION               = 107,
    API_KEY                              = 108,
    BLOCK_AMOUNT_GET                     = 109,
    WALLET_TIME_SYNC                     = 110,
    ELECTION_CONFLICT                    = 111,
    UNEXPECTED                           = 112,
    INVALID_PRIVATE_KEY                  = 113,
    NODE_STATUS                          = 114,
    BLOCK_HEIGHT_DUPLICATED              = 115,
    KEEPLIVE_ACK                         = 116,
    NODE_ACCOUNT_DUPLICATED              = 117,
    SUBSCRIPTION_EVENT                   = 118,
    EXTENSIONS_BROKEN_STREAM             = 119,
    EXTENSION_TYPE                       = 120,
    EXTENSION_PARSE_UNKNOWN              = 121,
    CONFIG_APP_VERSION                   = 122,
    WEBSOCKET_ACCEPT                     = 123,
    WEBSOCKET_CLOSE                      = 124,
    WEBSOCKET_SEND                       = 125,
    WEBSOCKET_RECEIVE                    = 126,
    WEBSOCKET_QUEUE_OVERFLOW             = 127,
    UTF8_CHECK                           = 128,
    UTF8_CONTROL_CHARACTER               = 129,
    HANDSHAKE_TIMESTAMP                  = 130,
    LEDGER_VERSION                       = 131,
    EXTENSION_VALUE                      = 132,
    RECEIVABLE_AMOUNT                    = 133,
    SEND_AMOUNT                          = 134,

    // json parsing errors: 200 ~ 299
    JSON_GENERIC                                    = 200,
    JSON_BLOCK_TYPE                                 = 201,
    JSON_BLOCK_OPCODE                               = 202,
    JSON_BLOCK_CREDIT                               = 203,
    JSON_BLOCK_COUNTER                              = 204,
    JSON_BLOCK_TIMESTAMP                            = 205,
    JSON_BLOCK_HEIGHT                               = 206,
    JSON_BLOCK_ACCOUNT                              = 207,
    JSON_BLOCK_PREVIOUS                             = 208,
    JSON_BLOCK_REPRESENTATIVE                       = 209,
    JSON_BLOCK_BALANCE                              = 210,
    JSON_BLOCK_LINK                                 = 211,
    JSON_BLOCK_EXTENSIONS_LENGTH                    = 212,
    JSON_BLOCK_EXTENSIONS_EMPTY                     = 213,
    JSON_BLOCK_EXTENSION_TYPE                       = 214,
    JSON_BLOCK_EXTENSION_VALUE                      = 215,
    JSON_BLOCK_SIGNATURE                            = 216,
    JSON_BLOCK_PRICE                                = 217,
    JSON_BLOCK_BEGIN_TIME                           = 218,
    JSON_BLOCK_END_TIME                             = 219,
    JSON_BLOCK_EXTENSIONS_RAW                       = 220,
    JSON_BLOCK_EXTENSION_FORMAT                     = 221,
    JSON_BLOCK_EXTENSION_ALIAS_OP                   = 222,
    JSON_BLOCK_EXTENSION_ALIAS_OP_VALUE             = 223,
    JSON_BLOCK_EXTENSION_TOKEN_OP                   = 224,
    JSON_BLOCK_EXTENSION_TOKEN_TYPE                 = 225,
    JSON_BLOCK_EXTENSION_TOKEN_NAME                 = 226,
    JSON_BLOCK_EXTENSION_TOKEN_SYMBOL               = 227,
    JSON_BLOCK_EXTENSION_TOKEN_INIT_SUPPLY          = 228,
    JSON_BLOCK_EXTENSION_TOKEN_CAP_SUPPLY           = 229,
    JSON_BLOCK_EXTENSION_TOKEN_DECIMALS             = 230,
    JSON_BLOCK_EXTENSION_TOKEN_BURNABLE             = 231,
    JSON_BLOCK_EXTENSION_TOKEN_MINTABLE             = 232,
    JSON_BLOCK_EXTENSION_TOKEN_CIRCULABLE           = 233,
    JSON_BLOCK_EXTENSION_TOKEN_BASE_URI             = 234,
    JSON_BLOCK_EXTENSION_TOKEN_TO                   = 235,
    JSON_BLOCK_EXTENSION_TOKEN_VALUE                = 236,
    JSON_BLOCK_EXTENSION_TOKEN_URI                  = 237,
    JSON_BLOCK_EXTENSION_TOKEN_CHAIN                = 238,
    JSON_BLOCK_EXTENSION_TOKEN_ADDRESS              = 239,
    JSON_BLOCK_EXTENSION_TOKEN_SOURCE               = 240,
    JSON_BLOCK_EXTENSION_TOKEN_FROM                 = 241,
    JSON_BLOCK_EXTENSION_TOKEN_BLOCK_HEIGHT         = 242,
    JSON_BLOCK_EXTENSION_TOKEN_TX_HASH              = 243,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_SUBOP           = 244,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_TOKEN_OFFER     = 245,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_TOKEN_WANT      = 246,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE_OFFER     = 247,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE_WANT      = 248,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_MIN_OFFER       = 249,


    JSON_CONFIG_VERSION                  = 250,
    JSON_CONFIG_RPC                      = 251,
    JSON_CONFIG_NODE                     = 252,
    JSON_CONFIG_NODE_VERSION             = 253,
    JSON_CONFIG_LOG                      = 254,
    JSON_CONFIG_LOG_VERSION              = 255,
    JSON_CONFIG_LOG_TO_CERR              = 256,
    JSON_CONFIG_LOG_MAX_SIZE             = 257,
    JSON_CONFIG_LOG_ROTATION_SIZE        = 258,
    JSON_CONFIG_LOG_FLUSH                = 259,
    JSON_CONFIG_NODE_PORT                = 260,
    JSON_CONFIG_NODE_IO_THREADS          = 261,
    JSON_CONFIG_LOG_NETWORK              = 262,
    JSON_CONFIG_LOG_NETWORK_SEND         = 263,
    JSON_CONFIG_LOG_NETWORK_RECEIVE      = 264,
    JSON_CONFIG_LOG_MESSAGE              = 265,
    JSON_CONFIG_LOG_MESSAGE_HANDSHAKE    = 266,
    JSON_CONFIG_PRECONFIGURED_PEERS      = 267,
    JSON_CONFIG_RPC_ENABLE               = 268,
    JSON_CONFIG_RPC_VERSION              = 269,
    JSON_CONFIG_RPC_ADDRESS              = 270,
    JSON_CONFIG_RPC_PORT                 = 271,
    JSON_CONFIG_RPC_ENABLE_CONTROL       = 272,
    JSON_CONFIG_RPC_WHITELIST            = 273,
    JSON_CONFIG_LOG_RPC                  = 274,
    JSON_CONFIG_WALLET_VERSION           = 275,
    JSON_CONFIG_WALLET_SERVER            = 276,
    JSON_CONFIG_CALLBACK_URL             = 277,
    JSON_CONFIG_WALLET_PRECONFIGURED_REP = 278,
    JSON_CONFIG_REWARD_TO                = 279,
    JSON_CONFIG_DAILY_REWARD_TIMES       = 280,
    JSON_CONFIG_ONLINE_STATS_URL         = 281,
    JSON_CONFIG_INVITED_REPS_URL         = 282,
    JSON_CONFIG_WALLET                   = 283,
    JSON_CONFIG_AIRDROP_MISS             = 284,
    JSON_CONFIG_AUTO_CREDIT              = 285,
    JSON_CONFIG_AUTO_RECEIVE             = 286,
    JSON_CONFIG_RECEIVE_MINIMUM          = 287,
    JSON_CONFIG_ENABLE_RICH_LIST         = 288,
    JSON_CONFIG_ENABLE_DELEGATOR_LIST    = 289,
    JSON_CONFIG_APP_VERSION              = 290,
    JSON_CONFIG_APP_NODE_GATEWAY         = 291,
    JSON_CONFIG_APP_WS_ADDRESS           = 292,
    JSON_CONFIG_APP_WS_PORT              = 293,
    JSON_CONFIG_APP_WS                   = 294,
    JSON_CONFIG_APP_WS_ENABLE            = 295,
    JSON_CONFIG_APP                      = 296,
    JSON_CONFIG_NODE_ADDRESS             = 297,

    // RPC errors: 300 ~ 399
    RPC_GENERIC                     = 300,
    RPC_JSON                        = 301,
    RPC_JSON_DEPTH                  = 302,
    RPC_HTTP_BODY_SIZE              = 303,
    RPC_NOT_LOCALHOST               = 304,
    RPC_MISS_FIELD_BLOCK            = 305,
    RPC_MISS_FIELD_ACCOUNT          = 306,
    RPC_INVALID_FIELD_ACCOUNT       = 307,
    RPC_ACCOUNT_NOT_EXIST           = 308,
    RPC_MISS_FIELD_TYPE             = 309,
    RPC_INVALID_FIELD_TYPE          = 310,
    RPC_MISS_FIELD_HEIGHT           = 311,
    RPC_INVALID_FIELD_HEIGHT        = 312,
    RPC_MISS_FIELD_PREVIOUS         = 313,
    RPC_INVALID_FIELD_PREVIOUS      = 314,
    RPC_UNKNOWN_ACTION              = 315,
    RPC_MISS_FIELD_TIMESTAMP        = 316,
    RPC_INVALID_FIELD_TIMESTAMP     = 317,
    RPC_MISS_FIELD_SIGNATURE        = 318,
    RPC_INVALID_FIELD_SIGNATURE     = 319,
    RPC_ENABLE_CONTROL              = 320,
    RPC_MISS_FIELD_COUNT            = 321,
    RPC_INVALID_FIELD_COUNT         = 322,
    RPC_MISS_FIELD_HASH             = 323,
    RPC_INVALID_FIELD_HASH          = 324,
    RPC_INVALID_FIELD_ROOT          = 325,
    RPC_MISS_FIELD_TO               = 326,
    RPC_INVALID_FIELD_TO            = 327,
    RPC_MISS_FIELD_AMOUNT           = 328,
    RPC_INVALID_FIELD_AMOUNT        = 329,
    RPC_INVALID_FIELD_REP           = 330,
    RPC_MISS_FIELD_EVENT            = 331,
    RPC_MISS_FIELD_NEXT             = 332,
    RPC_INVALID_FIELD_NEXT          = 333,
    RPC_MISS_FIELD_ACCOUNT_TYPES    = 334,
    RPC_INVALID_FIELD_ACCOUNT_TYPES = 335,
    RPC_ACTION_NOT_ALLOWED          = 336,
    RPC_INVALID_FIELD_CLIENT_ID     = 337,
    RPC_MISS_FIELD_SERVICE          = 338,
    RPC_INVALID_FIELD_SERVICE       = 339,
    RPC_MISS_FIELD_FILTERS          = 340,
    RPC_INVALID_FIELD_FILTERS       = 341,
    RPC_MISS_FIELD_NAME             = 342,
    RPC_INVALID_FIELD_NAME          = 343,
    RPC_MISS_FIELD_DNS              = 344,
    RPC_INVALID_FIELD_DNS           = 345,

    // Block process errors: 400 ~ 499
    BLOCK_PROCESS_GENERIC                     = 400,
    BLOCK_PROCESS_SIGNATURE                   = 401,
    BLOCK_PROCESS_EXISTS                      = 402,
    BLOCK_PROCESS_GAP_PREVIOUS                = 403,
    BLOCK_PROCESS_GAP_RECEIVE_SOURCE          = 404,
    BLOCK_PROCESS_GAP_REWARD_SOURCE           = 405,
    BLOCK_PROCESS_PREVIOUS                    = 406,
    BLOCK_PROCESS_OPCODE                      = 407,
    BLOCK_PROCESS_CREDIT                      = 408,
    BLOCK_PROCESS_COUNTER                     = 409,
    BLOCK_PROCESS_TIMESTAMP                   = 410,
    BLOCK_PROCESS_BALANCE                     = 411,
    BLOCK_PROCESS_UNRECEIVABLE                = 412,
    BLOCK_PROCESS_UNREWARDABLE                = 413,
    BLOCK_PROCESS_PRUNED                      = 414,
    BLOCK_PROCESS_FORK                        = 415,
    BLOCK_PROCESS_TYPE_MISMATCH               = 416,
    BLOCK_PROCESS_REPRESENTATIVE              = 417,
    BLOCK_PROCESS_LINK                        = 418,
    BLOCK_PROCESS_LEDGER_BLOCK_PUT            = 419,
    BLOCK_PROCESS_LEDGER_SUCCESSOR_SET        = 420,
    BLOCK_PROCESS_LEDGER_BLOCK_GET            = 421,
    BLOCK_PROCESS_TYPE_UNKNOWN                = 422,
    BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_PUT  = 423,
    BLOCK_PROCESS_LEDGER_RECEIVABLE_INFO_DEL  = 424,
    BLOCK_PROCESS_ACCOUNT_EXCEED_TRANSACTIONS = 425,
    BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_PUT  = 426,
    BLOCK_PROCESS_LEDGER_REWARDABLE_INFO_DEL  = 427,
    BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_PUT     = 428,
    BLOCK_PROCESS_LEDGER_INCONSISTENT         = 429,
    BLOCK_PROCESS_LEDGER_BLOCK_DEL            = 430,
    BLOCK_PROCESS_LEDGER_ROLLBACK_BLOCK_PUT   = 431,
    BLOCK_PROCESS_LEDGER_ACCOUNT_INFO_DEL     = 432,
    BLOCK_PROCESS_LEDGER_FORK_DEL             = 433,
    BLOCK_PROCESS_LEDGER_FORK_GET             = 434,
    BLOCK_PROCESS_LEDGER_FORK_PUT             = 435,

    BLOCK_PROCESS_ROLLBACK_REWARDED          = 488,
    BLOCK_PROCESS_CONFIRM_BLOCK_MISS         = 489,
    BLOCK_PROCESS_ROLLBACK_SOURCE_PRUNED     = 490,
    BLOCK_PROCESS_ROLLBACK_NOT_EQUAL_TO_HEAD = 491,
    BLOCK_PROCESS_ROLLBACK_RECEIVED          = 492,
    BLOCK_PROCESS_ROLLBACK_NON_HEAD          = 493,
    BLOCK_PROCESS_ROLLBACK_TAIL              = 494,
    BLOCK_PROCESS_ROLLBACK_IGNORE            = 495,
    BLOCK_PROCESS_PREPEND_IGNORE             = 496,
    BLOCK_PROCESS_UNKNOWN_OPERATION          = 497,
    BLOCK_PROCESS_CONTINUE                   = 498,
    BLOCK_PROCESS_WAIT                       = 499,

    BOOTSTRAP_GENERIC         = 500,
    BOOTSTRAP_PEER            = 501,
    BOOTSTRAP_RESET           = 502,
    BOOTSTRAP_ATTACK          = 503,
    BOOTSTRAP_CONNECT         = 504,
    BOOTSTRAP_TYPE            = 505,
    BOOTSTRAP_SEND            = 506,
    BOOTSTRAP_RECEIVE         = 507,
    BOOTSTRAP_ACCOUNT         = 508,
    BOOTSTRAP_FORK_LENGTH     = 509,
    BOOTSTRAP_FORK_BLOCK      = 510,
    BOOTSTRAP_SIZE            = 511,
    BOOTSTRAP_MESSAGE_TYPE    = 512,
    BOOTSTRAP_SLOW_CONNECTION = 513,


    APP_GENERIC                         = 600,
    APP_INVALID_GATEWAY_URL             = 601,
    APP_PROCESS_GAP_PREVIOUS            = 602,
    APP_PROCESS_LEDGER_ACCOUNT_PUT      = 603,
    APP_PROCESS_LEDGER_BLOCK_PUT        = 604,
    APP_PROCESS_PRUNED                  = 605,
    APP_PROCESS_PREVIOUS_MISMATCH       = 606,
    APP_PROCESS_LEDGER_SUCCESSOR_SET    = 607,
    APP_PROCESS_FORK                    = 608,
    APP_PROCESS_CONFIRMED_FORK          = 609,
    APP_PROCESS_EXIST                   = 610,
    APP_PROCESS_CONFIRMED               = 611,
    APP_PROCESS_ROLLBACK_ACCOUNT_MISS   = 612,
    APP_PROCESS_ROLLBACK_NON_HEAD       = 613,
    APP_PROCESS_ROLLBACK_CONFIRMED      = 614,
    APP_PROCESS_ROLLBACK_BLOCK_PUT      = 615,
    APP_PROCESS_LEDGER_BLOCK_DEL        = 616,
    APP_PROCESS_LEDGER_ACCOUNT_DEL      = 617,
    APP_PROCESS_CONFIRM_REQUIRED        = 618,
    APP_PROCESS_LEDGER_PUT              = 619,
    APP_PROCESS_LEDGER_DEL              = 620,
    APP_PROCESS_HALT                    = 621,
    APP_PROCESS_WAITING                 = 622,
    APP_PROCESS_UNEXPECTED              = 623,

    APP_RPC_MISS_FIELD_TRACE            = 650,
    APP_RPC_INVALID_FIELD_TRACE         = 651,


    ALIAS_GENERIC                       = 700,
    ALIAS_RESERVED_CHARACTOR_AT         = 701,
    ALIAS_RESERVED_CHARACTOR_UNDERSCORE = 702,
    ALIAS_OP_INVALID                    = 703,
    ALIAS_OP_UNKNOWN                    = 704,
    ALIAS_MULTI_EXTENSIONS              = 705,
    ALIAS_LEDGER_GET                    = 706,
    ALIAS_LEDGER_PUT                    = 707,

    TOKEN_GENERIC                       = 800,
    TOKEN_OP_INVALID                    = 801,
    TOKEN_OP_UNKNOWN                    = 802,
    TOKEN_TYPE_INVALID                  = 803,
    TOKEN_TYPE_UNKNOWN                  = 804,
    TOKEN_NAME_UTF8_CHECK               = 805,
    TOKEN_SYMBOL_UTF8_CHECK             = 806,
    TOKEN_CAP_SUPPLY_EXCEEDED           = 807,
    TOKEN_BASE_URI_UTF8_CHECK           = 808,
    TOKEN_URI_UTF8_CHECK                = 809,
    TOKEN_CHAIN_INVALID                 = 810,
    TOKEN_CHAIN_UNKNOWN                 = 811,
    TOKEN_SOURCE_INVALID                = 812,
    TOKEN_SOURCE_UNKNOWN                = 813,
    TOKEN_CREATION_DATA_MISS            = 814,
    TOKEN_SWAP_SUB_DATA_MISS            = 815,
    TOKEN_SWAP_VALUE_OFFER              = 816,
    TOKEN_SWAP_VALUE_WANT               = 817,
    TOKEN_SWAP_MIN_OFFER                = 818,
    TOKEN_SWAP_MAX_OFFER                = 819,
    TOKEN_SWAP_PAIR_EQUAL               = 820,
    TOKEN_SWAP_SUB_OP_INVALID           = 821,
    TOKEN_SWAP_SUB_OP_UNKNOWN           = 822,
    TOKEN_SWAP_MAKER                    = 823,
    TOKEN_SWAP_SHARE                    = 824,
    TOKEN_SWAP_ORDER_HEIGHT             = 825,
    TOKEN_SWAP_TRADE_HEIGHT             = 826,
    TOKEN_SWAP_INQUIRY_HEIGHT           = 827,
    TOKEN_SWAP_TAKER                    = 828,
    TOKEN_SWAP_TAKE_HEIGHT              = 829,
    TOKEN_UNMAP_CHAIN                   = 830,
    TOKEN_UNMAP_TO                      = 831,
    TOKEN_WRAP_CHAIN                    = 832,
    TOKEN_WRAP_TO                       = 833,
    TOKEN_VALUE                         = 834,
    TOKEN_MINT_TO                       = 835,
    TOKEN_SEND_TO                       = 836,
    TOKEN_BLOCK_HEIGHT                  = 837,
    TOKEN_UNWRAP_CHAIN                  = 838,
    TOKEN_SWAP_MAIN_ACCOUNT             = 839,
    TOKEN_CAP_SUPPLY                    = 840,
    TOKEN_LEDGER_GET                    = 841,
    TOKEN_LEDGER_PUT                    = 842,
    TOKEN_SWAP_ACK_HEIGHT               = 843,
    TOKEN_MULTI_EXTENSIONS              = 844,
    TOKEN_RECREATE                      = 845,
    TOKEN_NOT_CREATED                   = 846,
    TOKEN_TOTAL_SUPPLY_OVERFLOW         = 847,
    TOKEN_UNMINTABLE                    = 848,
    TOKEN_ID_REMINT                     = 849,
    TOKEN_UNBURNABLE                    = 850,
    TOKEN_BURN_MORE_THAN_BALANCE        = 851,
    TOKEN_ID_NOT_OWNED                  = 852,
    TOKEN_BALANCE_IS_EMPTY              = 853,
    TOKEN_BALANCE_IS_ZERO               = 854,
    TOKEN_UNCIRCULABLE                  = 855,
    TOKEN_SEND_MORE_THAN_BALANCE        = 856,
    TOKEN_UNRECEIVABLE                  = 857,
    TOKEN_SOURCE_TXN_HASH               = 858,
    TOKEN_RECEIVE_FROM                  = 859,
    TOKEN_RECEIVE_BLOCK_HEIGHT          = 860,
    TOKEN_RECEIVE_VALUE                 = 861,
    TOKEN_RECEIVE_SOURCE                = 862,
    TOKEN_BALANCE_OVERFLOW              = 863,
    TOKEN_SWAP_TIMESTAMP                = 864,
    TOKEN_SWAP_NO_ENOUGH_BALANCE        = 865,
    TOKEN_SWAP_MAIN_ACCOUNT_EMPTY       = 866,
    TOKEN_SWAP_ORDERS_EXCEEDED          = 867,
    TOKEN_SWAP_WITH_SELF                = 868,
    TOKEN_SWAP_ORDER_MISS               = 869,
    TOKEN_SWAP_INQUIRY_VALUE            = 870,
    TOKEN_SWAP_WITH_MAIN_ACCOUNT        = 871,
    TOKEN_SWAP_INQUIRY_WAITING_MISS     = 872,
    TOKEN_SWAP_TIMEOUT                  = 873,
    TOKEN_SWAP_STATUS_UNEXPECTED        = 874,
    TOKEN_SWAP_TAKE_WAITING_MISS        = 875,
    TOKEN_SWAP_TAKE_ACK_VALUE           = 876,
    TOKEN_SWAP_ORDER_FINISHED           = 877,
    TOKEN_SWAP_MISS                     = 878,
    TOKEN_SWAP_ACCOUNT_NOT_MAKER        = 879,
    TOKEN_SWAP_TAKE_NACK_HEIGHT         = 880,

    JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAX_OFFER       = 1000,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_TIMEOUT         = 1001,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAKER           = 1002,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_ORDER_HEIGHT    = 1003,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_TRADE_HEIGHT    = 1004,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_VALUE           = 1005,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_SHARE           = 1006,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_TAKER           = 1007,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_INQUIRY_HEIGHT  = 1008,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_SIGNATURE       = 1009,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_TAKE_HEIGHT     = 1010,
    JSON_BLOCK_EXTENSION_TOKEN_TO_CHAIN             = 1011,
    JSON_BLOCK_EXTENSION_TOKEN_TO_ACCOUNT           = 1012,
    JSON_BLOCK_EXTENSION_TOKEN_UNWRAP_CHAIN         = 1013,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_MAIN_ACCOUNT    = 1014,
    JSON_BLOCK_EXTENSION_TOKEN_UNMAP_EXTRA_DATA     = 1015,
    JSON_BLOCK_EXTENSION_TOKEN_SWAP_ACK_HEIGHT      = 1016,
    
    MAX = 1100
};

std::string ErrorString(rai::ErrorCode);

}  // namespace rai

#define IF_NOT_SUCCESS_RETURN(error_code)      \
    if (error_code != rai::ErrorCode::SUCCESS) \
    {                                          \
        return error_code;                     \
    }

#define IF_NOT_SUCCESS_RETURN_VOID(error_code) \
    if (error_code != rai::ErrorCode::SUCCESS) \
    {                                          \
        return;                                \
    }

#define IF_NOT_SUCCESS_BREAK(error_code)       \
    if (error_code != rai::ErrorCode::SUCCESS) \
    {                                          \
        break;                                 \
    }

