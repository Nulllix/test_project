/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_pkt.h>

#include <errno.h>
#include <stdio.h>

LOG_MODULE_REGISTER(main_code, CONFIG_LOG_DEFAULT_LEVEL);

/******************************************************************************
 * PRIVATE DEFINITIONS
 ******************************************************************************/
#define DEFAULT_IPV6_ADDR \
    { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x1 } } }

#define DEFAULT_IPV6_PORT 4242 /**< The default IPv6 port used by the system. */

#define LISTEN_THREAD_PRIORITY 4 /**< Listen thread priority. */
#define LISTEN_THREAD_STACK_SIZE 4096 /**< Listen thread stack size. */

/******************************************************************************
 * PRIVATE DATA
 ******************************************************************************/
static struct in6_addr ipv6_addr = DEFAULT_IPV6_ADDR; /**< This is a global variable representing the default IPv6 address. */
static struct k_thread listen_thread_handler; /**< This variable is used to define the handler of the thread that listens and receives incoming messages. */
static K_THREAD_STACK_DEFINE(listen_thread_stack, LISTEN_THREAD_STACK_SIZE); /**< Static listen thread stack. */
static uint8_t rx_buf[NET_IPV6_MTU] = {0}; /**< Static buffer to store received IPv6 packets. */

/******************************************************************************
 * PRIVATE FUNCTIONS
 ******************************************************************************/

/** @brief Callback function for received TCP data.
 *
 * This callback function is called by the network stack when a TCP datagram has been received on the specified context. It reads the data from the packet buffer and logs it to the console.
 *
 * @param[in] context The network context that was used to receive the data.
 * @param[in] pkt The received network packet containing the data.
 * @param[in] ip_hdr Pointer to the IP header of the received datagram.
 * @param[in] proto_hdr Pointer to the protocol header of the received datagram.
 * @param[in] status Status code indicating whether the operation succeeded or failed. Zero value means success.
 * @param[in] user_data Optional user-defined data passed to the callback during initialization. Could be NULL.
 */
static void tcp_received_cb (
    struct net_context *context,
    struct net_pkt *pkt,
    union net_ip_header *ip_hdr,
    union net_proto_header *proto_hdr,
    int status,
    void *user_data
) {
    int reply_len = net_pkt_remaining_data(pkt);

    if (reply_len == 0) {
        return;
    }

    int read_status = net_pkt_read(pkt, rx_buf, reply_len);
    if (read_status < 0) {
        LOG_ERR("Cannot read packet: %d", read_status);
        return;
    }

    LOG_INF("TCP RX [%d]: %s", reply_len, rx_buf);
}

/**
 * @brief This function is called when a TCP connection has been accepted. It sets the receiving callback and disables accepting on the new socket to prevent further incoming connections while still in this callback.
 *
 * @param[in] context The NetContext that represents the newly connected socket.
 * @param[in] addr The address of the remote host that initiated the connection.
 * @param[in] addrlen The length of the address information structure pointed by addr.
 * @param[in] error A negative value indicating an error during the accept operation, or zero for successful completion.
 * @param[in] user_data User data passed to the net_context_listen() call that started the listening process.
 */
static void tcp_accepted_cb(
    struct net_context *context,
    struct sockaddr *addr,
    socklen_t addrlen,
    int error,
    void *user_data
) {
    LOG_INF("Accept called, contex %p error %d", context, error);

    net_context_set_accepting(context, false); // Try without

    int status = net_context_recv(context, tcp_received_cb, K_NO_WAIT, NULL);
    if (status < 0) {
        LOG_ERR("Failed set recv callback (family %d)", net_context_get_family(context));
    }
}

/**
 * @brief Listen thread.
 *
 * @param p1    Unused pointer argument for future use cases
 * @param p2    Unused pointer argument for future use cases
 * @param p3    Unused pointer argument for future use cases
 */
static void listen_thread(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct net_context *tcp_ipv6_recv_cntx = NULL;struct sockaddr_in6 ipv6_soc_addr = {0};
    ipv6_soc_addr.sin6_family = AF_INET6;ipv6_soc_addr.sin6_port = htons(DEFAULT_IPV6_PORT);

    int status = net_context_get(AF_INET6, SOCK_STREAM, IPPROTO_TCP, &tcp_ipv6_recv_cntx);

    if (status < 0) {
        LOG_ERR("Cannot get network context for IPv6 TCP (%d)", status);
        return;
    }

    status = net_context_bind(tcp_ipv6_recv_cntx, (struct sockaddr *)&ipv6_soc_addr, sizeof(struct sockaddr_in6));
    if (status < 0) {
        LOG_ERR("Cannot bind IPv6 TCP port %d (%d)", ntohs(ipv6_soc_addr.sin6_port), status);
        return;
    }

    status = net_context_listen(tcp_ipv6_recv_cntx, 0);
    if (status < 0) {
        LOG_ERR("Cannot IPv6 TCP context how listen (%d)", status);
        return;
    }

    status = net_context_accept(tcp_ipv6_recv_cntx, tcp_accepted_cb, K_NO_WAIT, NULL);
    if (status < 0) {
        LOG_ERR("Failed accept TCP (family %d)", net_context_get_family(tcp_ipv6_recv_cntx));
        return;
    }

    while(1) {
        k_sleep(K_MSEC(1000));
    };
}

/******************************************************************************
 * PUBLIC FUNCTIONS
 ******************************************************************************/

/** @brief Main application entry point.
 */
int main(void) {
    // log_panic();

    LOG_INF("Run roadmap project");

    int status = net_addr_pton(AF_INET6, CONFIG_NET_CONFIG_MY_IPV6_ADDR, &ipv6_addr);
    if (status != 0) {
        LOG_ERR("Invalid IPv6 address %s", CONFIG_NET_CONFIG_MY_IPV6_ADDR);
        return -1;
    }

    struct net_if_addr *ifaddr;
    ifaddr = net_if_ipv6_addr_add(net_if_get_default(), &ipv6_addr, NET_ADDR_MANUAL, 0);
    if (ifaddr == NULL) {
        LOG_ERR("Failed add IPv6 addr");
        return -1;
    }

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
