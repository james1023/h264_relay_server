#ifndef ASIO_SSL_DTLS_MACRO_HELPER_HPP
#define ASIO_SSL_DTLS_MACRO_HELPER_HPP

#ifdef    ASIO_DTLS_USE_BOOST

#define ASIO_DTLS_DECL BOOST_ASIO_DECL
#define ASIO_DTLS_STATIC_CONSTANT BOOST_ASIO_STATIC_CONSTANT
#define ASIO_DTLS_SYNC_OP_VOID BOOST_ASIO_SYNC_OP_VOID
#define ASIO_DTLS_SYNC_OP_VOID_RETURN BOOST_ASIO_SYNC_OP_VOID_RETURN
#define ASIO_DTLS_MOVE_CAST BOOST_ASIO_MOVE_CAST
#define asio_dtls_handler_alloc_helpers boost_asio_handler_alloc_helpers
#define asio_dtls_handler_cont_helpers boost_asio_handler_cont_helpers
#define asio_dtls_handler_invoke_helpers boost_asio_handler_invoke_helpers
#define ASIO_DTLS_NOEXCEPT BOOST_ASIO_NOEXCEPT
#define ASIO_DTLS_HAS_BOOST_DATE_TIME
#define ASIO_DTLS_MOVE_ARG BOOST_ASIO_MOVE_ARG
#define ASIO_DTLS_HANDSHAKE_HANDLER_CHECK BOOST_ASIO_HANDSHAKE_HANDLER_CHECK
#define ASIO_DTLS_INITFN_RESULT_TYPE BOOST_ASIO_INITFN_RESULT_TYPE
#define ASIO_DTLS_READ_HANDLER_CHECK BOOST_ASIO_READ_HANDLER_CHECK
#define ASIO_DTLS_HANDLER_TYPE BOOST_ASIO_HANDLER_TYPE
#define ASIO_DTLS_BUFFERED_HANDSHAKE_HANDLER_CHECK BOOST_ASIO_BUFFERED_HANDSHAKE_HANDLER_CHECK
#define ASIO_DTLS_WRITE_HANDLER_CHECK BOOST_ASIO_WRITE_HANDLER_CHECK
#define ASIO_DTLS_HAS_MOVE BOOST_ASIO_HAS_MOVE

#else  // ASIO_DTLS_USE_BOOST

#define ASIO_DTLS_DECL ASIO_DECL
#define ASIO_DTLS_STATIC_CONSTANT ASIO_STATIC_CONSTANT
#define ASIO_DTLS_SYNC_OP_VOID ASIO_SYNC_OP_VOID
#define ASIO_DTLS_SYNC_OP_VOID_RETURN ASIO_SYNC_OP_VOID_RETURN
#define ASIO_DTLS_MOVE_CAST ASIO_MOVE_CAST
#define asio_dtls_handler_alloc_helpers asio_handler_alloc_helpers
#define asio_dtls_handler_cont_helpers asio_handler_cont_helpers
#define asio_dtls_handler_invoke_helpers asio_handler_invoke_helpers
#define ASIO_DTLS_NOEXCEPT ASIO_NOEXCEPT
#if defined(ASIO_HAS_BOOST_DATE_TIME)
#define ASIO_DTLS_HAS_BOOST_DATE_TIME
#endif
#define ASIO_DTLS_MOVE_ARG ASIO_MOVE_ARG
#define ASIO_DTLS_HANDSHAKE_HANDLER_CHECK ASIO_HANDSHAKE_HANDLER_CHECK
#define ASIO_DTLS_INITFN_RESULT_TYPE ASIO_INITFN_RESULT_TYPE
#define ASIO_DTLS_READ_HANDLER_CHECK ASIO_READ_HANDLER_CHECK
#define ASIO_DTLS_HANDLER_TYPE ASIO_HANDLER_TYPE
#define ASIO_DTLS_BUFFERED_HANDSHAKE_HANDLER_CHECK ASIO_BUFFERED_HANDSHAKE_HANDLER_CHECK
#define ASIO_DTLS_WRITE_HANDLER_CHECK ASIO_WRITE_HANDLER_CHECK
#define ASIO_DTLS_HAS_MOVE ASIO_HAS_MOVE

#endif // ASIO_DTLS_USE_BOOST

#ifdef ASIO_DTLS_USE_BOOST
#else  // ASIO_DTLS_USE_BOOST
#endif // ASIO_DTLS_USE_BOOST

#endif // ASIO_SSL_DTLS_MACRO_HELPER_HPP
