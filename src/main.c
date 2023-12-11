/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(main_code, CONFIG_LOG_DEFAULT_LEVEL);

/******************************************************************************
 * PRIVATE DEFINITIONS
 ******************************************************************************/
#define DEFAULT_IPV6_PORT 4242 /**< The default IPv6 port used by the system. */

#define LISTEN_THREAD_PRIORITY 4 /**< Listen thread priority. */
#define LISTEN_THREAD_STACK_SIZE 4096 /**< Listen thread stack size. */

#define SERVER_CERTIFICATE_TAG 1

#define CHECK_ERROR_WITH_RETURN(_condition_, _log_, _ret_value_)    \
    do {                                                            \
        if (_condition_) {                                          \
            LOG_ERR(_log_);                                         \
            return _ret_value_;                                     \
        }                                                           \
    }                                                               \
    while(0)

/******************************************************************************
 * PRIVATE DATA
 ******************************************************************************/
static struct k_thread listen_thread_handler; /**< This variable is used to define the handler of the thread that listens and receives incoming messages. */
static K_THREAD_STACK_DEFINE(listen_thread_stack, LISTEN_THREAD_STACK_SIZE); /**< Static listen thread stack. */
static int server_sock_desc = 0; /**< Server socket */

/**
 * @brief Server certificate
*/
static const unsigned char server_certificate[] = {
#include "echo-apps-cert.der.inc"
};

/**
 * @brief Private server key
*/
static const unsigned char private_key[] = {
#include "echo-apps-key.der.inc"
};

/******************************************************************************
 * PRIVATE FUNCTIONS
 ******************************************************************************/

/**
 * @brief Listen thread.
 *
 * @param sock_desc  Socket description
 * @param p2    Unused pointer argument for future use cases
 * @param p3    Unused pointer argument for future use cases
 */
static void listen_thread(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct sockaddr_in6 client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while(1) {
        int client_sock_desc = accept(server_sock_desc, (struct sockaddr *)&client_addr, &client_addr_len);
        CHECK_ERROR_WITH_RETURN(client_sock_desc < 0, "Failed accept socket", );

        LOG_INF("Socket accept");

        char buf[128] = {0};
        while (1) {
            int len = recv(client_sock_desc, buf, sizeof(buf), 0);

            if (len <= 0) {
                if (len < 0) {
                    LOG_ERR("recv error: %d", len);
                }
                break;
            }

            LOG_INF("RX: %s", buf);
            memset(buf, 0, sizeof(buf));
        }

        close(client_sock_desc);
        LOG_INF("Socket close");
    }
}

/**
 * @brief Init TLS
*/
int init_tls(void) {
    int status = tls_credential_add(
        SERVER_CERTIFICATE_TAG,
        TLS_CREDENTIAL_SERVER_CERTIFICATE,
        server_certificate,
        sizeof(server_certificate)
    );

    CHECK_ERROR_WITH_RETURN(status < 0, "Failed to add certificate", status);

    status = tls_credential_add(
        SERVER_CERTIFICATE_TAG,
        TLS_CREDENTIAL_PRIVATE_KEY,
        private_key,
        sizeof(private_key)
    );

    CHECK_ERROR_WITH_RETURN(status < 0, "Failed to add key", status);

    return 0;
}

/**
 * @brief Init socket
*/
int init_socket(void) {
    struct sockaddr_in6 bind_addr = {
        .sin6_family = AF_INET6,
        .sin6_port = htons(DEFAULT_IPV6_PORT),
    };

    sec_tag_t sec_tag_list[] = {
        SERVER_CERTIFICATE_TAG,
    };

    int status = net_addr_pton(AF_INET6, CONFIG_NET_CONFIG_MY_IPV6_ADDR, &bind_addr.sin6_addr);
    CHECK_ERROR_WITH_RETURN(status != 0, "Invalid IPv6 address", status);

    server_sock_desc = socket(AF_INET6, SOCK_STREAM, IPPROTO_TLS_1_2);
    CHECK_ERROR_WITH_RETURN(status < 0, "Failed create socket", status);
    
    status = setsockopt(
        server_sock_desc, 
        SOL_TLS, 
        TLS_SEC_TAG_LIST,
        sec_tag_list,
        sizeof(sec_tag_list)
    );
    CHECK_ERROR_WITH_RETURN(status < 0, "Failed set cert for socket", status);

    status = bind(server_sock_desc, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
    CHECK_ERROR_WITH_RETURN(status != 0, "Failed bind socket", status);

    status = listen(server_sock_desc, 1);
    CHECK_ERROR_WITH_RETURN(status != 0, "Failed listen socket", status);

    return 0;
}

/******************************************************************************
 * PUBLIC FUNCTIONS
 ******************************************************************************/

/** @brief Main application entry point.
 */
int main(void) {
    LOG_INF("Run roadmap project");

    int status = init_tls();
    CHECK_ERROR_WITH_RETURN(status != 0, "Failed init TLS", -1);

    status = init_socket();
    CHECK_ERROR_WITH_RETURN(status != 0, "Failed init socket", -1);

    k_thread_create(
        &listen_thread_handler,
        listen_thread_stack,
        K_THREAD_STACK_SIZEOF(listen_thread_stack),
        listen_thread,
        NULL,
        NULL,
        NULL,
        LISTEN_THREAD_PRIORITY,
        0,
        K_NO_WAIT
    );
}
