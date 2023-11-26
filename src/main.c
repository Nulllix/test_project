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

#define DEFAULT_IPV6_PORT 4242

#define LISTEN_THREAD_PRIORITY 4
#define LISTEN_THREAD_STACK_SIZE 4096

/******************************************************************************
 * PRIVATE DATA
 ******************************************************************************/
static struct in6_addr ipv6_addr = DEFAULT_IPV6_ADDR;
static struct k_thread listen_thread_handler;
static uint8_t rx_buf[NET_IPV6_MTU] = {0};
K_THREAD_STACK_DEFINE(listen_thread_stack, LISTEN_THREAD_STACK_SIZE);

/******************************************************************************
 * PRIVATE FUNCTIONS
 ******************************************************************************/

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

    net_pkt_unref(pkt);

    LOG_INF("TCP RX [%d]: %s", reply_len, rx_buf);
}

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

static void listen_thread(void *p1, void *p2, void *p3) {
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct net_context *tcp_ipv6_recv_cntx = NULL;
    struct sockaddr_in6 ipv6_soc_addr = {0};

    ipv6_soc_addr.sin6_family = AF_INET6;
    ipv6_soc_addr.sin6_port = htons(DEFAULT_IPV6_PORT);

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
