// Webthing-CPP
// SPDX-FileCopyrightText: 2023 Benno Waldhauer
// SPDX-License-Identifier: MIT

// MdnsService follows mdns.c example of Mattia Janssons mdns lib.
// also cf. https://github.com/mjansson/mdns 

#pragma once

// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")

#include <mdns.h>
#include <bw/webthing/utils.hpp>

#ifdef _WIN32
#include <iphlpapi.h>
#else
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#endif

namespace bw::webthing
{

static void mdns_log(std::string msg)
{
    logger::trace("MdnsService - " + msg);
}

const static int MAX_HOST_SIZE = 1025;

static char addrbuffer[64];
static char entrybuffer[256];
static char namebuffer[256];
static char sendbuffer[1024];
static mdns_record_txt_t txtbuffer[128];

static struct sockaddr_in service_address_ipv4;
static struct sockaddr_in6 service_address_ipv6;

static int has_ipv4;
static int has_ipv6;

static int is_tls_server;


static mdns_string_t ipv4_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in* addr,
                    size_t addrlen) {
    char host[MAX_HOST_SIZE] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, MAX_HOST_SIZE,
                        service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0) {
        if (addr->sin_port != 0)
            len = snprintf(buffer, capacity, "%s:%s", host, service);
        else
            len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int)capacity)
        len = (int)capacity - 1;
    mdns_string_t str;
    str.str = buffer;
    str.length = len;
    return str;
}

static mdns_string_t ipv6_address_to_string(char* buffer, size_t capacity, const struct sockaddr_in6* addr,
                    size_t addrlen) {
    char host[NI_MAXHOST] = {0};
    char service[NI_MAXSERV] = {0};
    int ret = getnameinfo((const struct sockaddr*)addr, (socklen_t)addrlen, host, NI_MAXHOST,
                        service, NI_MAXSERV, NI_NUMERICSERV | NI_NUMERICHOST);
    int len = 0;
    if (ret == 0) {
        if (addr->sin6_port != 0)
            len = snprintf(buffer, capacity, "[%s]:%s", host, service);
        else
            len = snprintf(buffer, capacity, "%s", host);
    }
    if (len >= (int)capacity)
        len = (int)capacity - 1;
    mdns_string_t str;
    str.str = buffer;
    str.length = len;
    return str;
}

static mdns_string_t ip_address_to_string(char* buffer, size_t capacity, const struct sockaddr* addr, size_t addrlen) {
    if (addr->sa_family == AF_INET6)
        return ipv6_address_to_string(buffer, capacity, (const struct sockaddr_in6*)addr, addrlen);
    return ipv4_address_to_string(buffer, capacity, (const struct sockaddr_in*)addr, addrlen);
}

static std::vector<std::string> get_addresses()
{
    std::vector<std::string> ips;

    #ifdef _WIN32

        PIP_ADAPTER_ADDRESSES ifaddresses = NULL;
        ULONG size = 0;
        DWORD ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, NULL, ifaddresses, &size);
        if (ret == ERROR_BUFFER_OVERFLOW)
        {
            ifaddresses = (IP_ADAPTER_ADDRESSES *)malloc(size);
            ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, NULL, ifaddresses, &size);
        }

        for (PIP_ADAPTER_ADDRESSES adapter = ifaddresses; adapter != NULL; adapter = adapter->Next)
        {
            for (PIP_ADAPTER_UNICAST_ADDRESS unicast = adapter->FirstUnicastAddress; unicast != NULL; unicast = unicast->Next)
            {
                sockaddr *sa = unicast->Address.lpSockaddr;
                char ip[INET6_ADDRSTRLEN];

                if (sa->sa_family == AF_INET)
                {
                    struct sockaddr_in *saddr = (struct sockaddr_in *)sa;
                    if (saddr->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
                        continue;

                    inet_ntop(AF_INET, &(saddr)->sin_addr, ip, INET_ADDRSTRLEN);
                    ips.push_back(ip);
                }
                else if (sa->sa_family == AF_INET6)
                {
                    struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)sa;
                    // Ignore link-local addresses
                    if (saddr->sin6_scope_id)
                        continue;

                    static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
                    static const unsigned char localhost_mapped[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};

                    if (!memcmp(saddr->sin6_addr.s6_addr, localhost, 16) ||
                        !memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16))
                    {
                        continue;
                    }

                    inet_ntop(AF_INET6, &((sockaddr_in6 *)sa)->sin6_addr, ip, INET6_ADDRSTRLEN);
                    ips.push_back(ip);
                }
            }
        }
        free(ifaddresses);
        
    #else
        struct ifaddrs* ifaddr = 0;
        struct ifaddrs* ifa = 0;

        if (getifaddrs(&ifaddr) >= 0)
        {
            char addr_buffer[64];
            for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
            {
                if(!ifa->ifa_addr)
                    continue;

                if(ifa->ifa_addr->sa_family == AF_INET)
                {
                    struct sockaddr_in* saddr = (struct sockaddr_in*)ifa->ifa_addr;
                    if (saddr->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
                        continue;
                    mdns_string_t ip = ip_address_to_string(addr_buffer, sizeof(addr_buffer), ifa->ifa_addr, sizeof(sockaddr_in));
                    ips.push_back(ip.str);

                }
                else if(ifa->ifa_addr->sa_family == AF_INET6)
                {
                    struct sockaddr_in6* saddr = (struct sockaddr_in6*)ifa->ifa_addr;
                    // Ignore link-local addresses
                    if (saddr->sin6_scope_id)
                        continue;

                    static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
                    static const unsigned char localhost_mapped[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};

                    if(!memcmp(saddr->sin6_addr.s6_addr, localhost, 16) ||
                       !memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16))
                    {
                        continue;
                    }

                    mdns_string_t ip = ip_address_to_string(addr_buffer, sizeof(addr_buffer), ifa->ifa_addr, sizeof(sockaddr_in6));
                    ips.push_back(ip.str);
                }
            }
        }
        freeifaddrs(ifaddr);
    #endif

    return ips;
}



struct MdnsService
{
    volatile bool run_requested = false;
    volatile bool running = false;
    
    // Data for our service including the mDNS records
    typedef struct {
        mdns_string_t service;
        mdns_string_t hostname;
        mdns_string_t service_instance;
        mdns_string_t hostname_qualified;
        struct sockaddr_in address_ipv4;
        struct sockaddr_in6 address_ipv6;
        int port;
        mdns_record_t record_ptr;
        mdns_record_t record_srv;
        mdns_record_t record_a;
        mdns_record_t record_aaaa;
        mdns_record_t txt_record[2];
    } service_t;
    
   
    // Callback handling questions incoming on service sockets
    static int service_callback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry,
                    uint16_t query_id, uint16_t ui16_rtype, uint16_t rclass, uint32_t ttl, const void* data,
                    size_t size, size_t name_offset, size_t name_length, size_t record_offset,
                    size_t record_length, void* user_data) {
        (void)sizeof(ttl);
        if (entry != MDNS_ENTRYTYPE_QUESTION)
            return 0;

        mdns_record_type rtype = static_cast<mdns_record_type>(ui16_rtype);

        const char dns_sd[] = "_services._dns-sd._udp.local.";
        const service_t* service = (const service_t*)user_data;

        mdns_string_t fromaddrstr = ip_address_to_string(addrbuffer, sizeof(addrbuffer), from, addrlen);

        size_t offset = name_offset;
        mdns_string_t name = mdns_string_extract(data, size, &offset, namebuffer, sizeof(namebuffer));

        const char* record_name = 0;
        if (rtype == MDNS_RECORDTYPE_PTR)
            record_name = "PTR";
        else if (rtype == MDNS_RECORDTYPE_SRV)
            record_name = "SRV";
        else if (rtype == MDNS_RECORDTYPE_A)
            record_name = "A";
        else if (rtype == MDNS_RECORDTYPE_AAAA)
            record_name = "AAAA";
        else if (rtype == MDNS_RECORDTYPE_TXT)
            record_name = "TXT";
        else if (rtype == MDNS_RECORDTYPE_ANY)
            record_name = "ANY";
        else
            return 0;
        mdns_log("Query " + std::string(record_name) + " " +  std::string(name.str));

        if ((name.length == (sizeof(dns_sd) - 1)) &&
            (strncmp(name.str, dns_sd, sizeof(dns_sd) - 1) == 0)) {
            if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype == MDNS_RECORDTYPE_ANY)) {
                // The PTR query was for the DNS-SD domain, send answer with a PTR record for the
                // service name we advertise, typically on the "<_service-name>._tcp.local." format

                // Answer PTR record reverse mapping "<_service-name>._tcp.local." to
                // "<hostname>.<_service-name>._tcp.local."
                mdns_record_t answer = {
                    name, MDNS_RECORDTYPE_PTR, {service->service}};

                // Send the answer, unicast or multicast depending on flag in query
                uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
                mdns_log("  --> answer " + std::string(answer.data.ptr.name.str) + " " + 
                    std::string(unicast ? "unicast" : "multicast"));

                if (unicast) {
                    mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                            query_id, rtype, name.str, name.length, answer, 0, 0, 0,
                                            0);
                } else {
                    mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0, 0,
                                                0);
                }
            }
        } else if ((name.length == service->service.length) &&
                (strncmp(name.str, service->service.str, name.length) == 0)) {
            if ((rtype == MDNS_RECORDTYPE_PTR) || (rtype == MDNS_RECORDTYPE_ANY)) {
                // The PTR query was for our service (usually "<_service-name._tcp.local"), answer a PTR
                // record reverse mapping the queried service name to our service instance name
                // (typically on the "<hostname>.<_service-name>._tcp.local." format), and add
                // additional records containing the SRV record mapping the service instance name to our
                // qualified hostname (typically "<hostname>.local.") and port, as well as any IPv4/IPv6
                // address for the hostname as A/AAAA records, and two test TXT records

                // Answer PTR record reverse mapping "<_service-name>._tcp.local." to
                // "<hostname>.<_service-name>._tcp.local."
                mdns_record_t answer = service->record_ptr;

                mdns_record_t additional[5] = {0};
                size_t additional_count = 0;

                // SRV record mapping "<hostname>.<_service-name>._tcp.local." to
                // "<hostname>.local." with port. Set weight & priority to 0.
                additional[additional_count++] = service->record_srv;

                // A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
                if (service->address_ipv4.sin_family == AF_INET)
                    additional[additional_count++] = service->record_a;
                if (service->address_ipv6.sin6_family == AF_INET6)
                    additional[additional_count++] = service->record_aaaa;

                // Add two test TXT records for our service instance name, will be coalesced into
                // one record with both key-value pair strings by the library
                additional[additional_count++] = service->txt_record[0];
                if(is_tls_server)
                    additional[additional_count++] = service->txt_record[1];

                // Send the answer, unicast or multicast depending on flag in query
                uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
                mdns_log("  --> answer " + std::string(service->record_ptr.data.ptr.name.str) + 
                    " (" + std::string(unicast ? "unicast" : "multicast") + ")");

                if (unicast) {
                    mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                            query_id, rtype, name.str, name.length, answer, 0, 0,
                                            additional, additional_count);
                } else {
                    mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
                                                additional, additional_count);
                }
            }
        } else if ((name.length == service->service_instance.length) &&
                (strncmp(name.str, service->service_instance.str, name.length) == 0)) {
            if ((rtype == MDNS_RECORDTYPE_SRV) || (rtype == MDNS_RECORDTYPE_ANY)) {
                // The SRV query was for our service instance (usually
                // "<hostname>.<_service-name._tcp.local"), answer a SRV record mapping the service
                // instance name to our qualified hostname (typically "<hostname>.local.") and port, as
                // well as any IPv4/IPv6 address for the hostname as A/AAAA records, and two test TXT
                // records

                // Answer PTR record reverse mapping "<_service-name>._tcp.local." to
                // "<hostname>.<_service-name>._tcp.local."
                mdns_record_t answer = service->record_srv;

                mdns_record_t additional[5] = {0};
                size_t additional_count = 0;

                // A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
                if (service->address_ipv4.sin_family == AF_INET)
                    additional[additional_count++] = service->record_a;
                if (service->address_ipv6.sin6_family == AF_INET6)
                    additional[additional_count++] = service->record_aaaa;

                // Add two test TXT records for our service instance name, will be coalesced into
                // one record with both key-value pair strings by the library
                additional[additional_count++] = service->txt_record[0];
                if(is_tls_server)
                    additional[additional_count++] = service->txt_record[1];

                // Send the answer, unicast or multicast depending on flag in query
                uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
                mdns_log("  --> answer " + std::string(service->record_srv.data.srv.name.str) + 
                    " port " + std::to_string(service->port) + " (" + (unicast ? "unicast" : "multicast") + ")");

                if (unicast) {
                    mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                            query_id, rtype, name.str, name.length, answer, 0, 0,
                                            additional, additional_count);
                } else {
                    mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
                                                additional, additional_count);
                }
            }
        } else if ((name.length == service->hostname_qualified.length) &&
                (strncmp(name.str, service->hostname_qualified.str, name.length) == 0)) {
            if (((rtype == MDNS_RECORDTYPE_A) || (rtype == MDNS_RECORDTYPE_ANY)) &&
                (service->address_ipv4.sin_family == AF_INET)) {
                // The A query was for our qualified hostname (typically "<hostname>.local.") and we
                // have an IPv4 address, answer with an A record mappiing the hostname to an IPv4
                // address, as well as any IPv6 address for the hostname, and two test TXT records

                // Answer A records mapping "<hostname>.local." to IPv4 address
                mdns_record_t answer = service->record_a;

                mdns_record_t additional[5] = {0};
                size_t additional_count = 0;

                // AAAA record mapping "<hostname>.local." to IPv6 addresses
                if (service->address_ipv6.sin6_family == AF_INET6)
                    additional[additional_count++] = service->record_aaaa;

                // Add two test TXT records for our service instance name, will be coalesced into
                // one record with both key-value pair strings by the library
                additional[additional_count++] = service->txt_record[0];
                if(is_tls_server)
                    additional[additional_count++] = service->txt_record[1];

                // Send the answer, unicast or multicast depending on flag in query
                uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
                mdns_string_t addrstr = ip_address_to_string(
                    addrbuffer, sizeof(addrbuffer), (struct sockaddr*)&service->record_a.data.a.addr,
                    sizeof(service->record_a.data.a.addr));
                mdns_log("  --> answer " + std::string(service->record_a.name.str) + 
                    " IPv4 " + std::string(addrstr.str) + " (" + std::string(unicast ? "unicast" : "multicast") + ")");

                if (unicast) {
                    mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                            query_id, rtype, name.str, name.length, answer, 0, 0,
                                            additional, additional_count);
                } else {
                    mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
                                                additional, additional_count);
                }
            } else if (((rtype == MDNS_RECORDTYPE_AAAA) || (rtype == MDNS_RECORDTYPE_ANY)) &&
                    (service->address_ipv6.sin6_family == AF_INET6)) {
                // The AAAA query was for our qualified hostname (typically "<hostname>.local.") and we
                // have an IPv6 address, answer with an AAAA record mappiing the hostname to an IPv6
                // address, as well as any IPv4 address for the hostname, and two test TXT records

                // Answer AAAA records mapping "<hostname>.local." to IPv6 address
                mdns_record_t answer = service->record_aaaa;

                mdns_record_t additional[5] = {0};
                size_t additional_count = 0;

                // A record mapping "<hostname>.local." to IPv4 addresses
                if (service->address_ipv4.sin_family == AF_INET)
                    additional[additional_count++] = service->record_a;

                // Add two test TXT records for our service instance name, will be coalesced into
                // one record with both key-value pair strings by the library
                additional[additional_count++] = service->txt_record[0];
                if(is_tls_server)
                    additional[additional_count++] = service->txt_record[1];

                // Send the answer, unicast or multicast depending on flag in query
                uint16_t unicast = (rclass & MDNS_UNICAST_RESPONSE);
                mdns_string_t addrstr =
                    ip_address_to_string(addrbuffer, sizeof(addrbuffer),
                                        (struct sockaddr*)&service->record_aaaa.data.aaaa.addr,
                                        sizeof(service->record_aaaa.data.aaaa.addr));
                mdns_log("  --> answer " + std::string(service->record_aaaa.name.str) + 
                    " IPv6 " + std::string(addrstr.str) + " (" + std::string(unicast ? "unicast" : "multicast") + ")");

                if (unicast) {
                    mdns_query_answer_unicast(sock, from, addrlen, sendbuffer, sizeof(sendbuffer),
                                            query_id, rtype, name.str, name.length, answer, 0, 0,
                                            additional, additional_count);
                } else {
                    mdns_query_answer_multicast(sock, sendbuffer, sizeof(sendbuffer), answer, 0, 0,
                                                additional, additional_count);
                }
            }
        }
        return 0;
    }

    // Open sockets for sending one-shot multicast queries from an ephemeral port
    static int open_client_sockets(int* sockets, int max_sockets, int port) {
        // When sending, each socket can only send to one network interface
        // Thus we need to open one socket for each interface and address family
        int num_sockets = 0;

    #ifdef _WIN32

        IP_ADAPTER_ADDRESSES* adapter_address = 0;
        ULONG address_size = 8000;
        unsigned int ret;
        unsigned int num_retries = 4;
        do {
            adapter_address = (IP_ADAPTER_ADDRESSES*)malloc(address_size);
            ret = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_ANYCAST, 0,
                                    adapter_address, &address_size);
            if (ret == ERROR_BUFFER_OVERFLOW) {
                free(adapter_address);
                adapter_address = 0;
                address_size *= 2;
            } else {
                break;
            }
        } while (num_retries-- > 0);

        if (!adapter_address || (ret != NO_ERROR)) {
            free(adapter_address);
            mdns_log("Failed to get network adapter addresses");
            return num_sockets;
        }

        int first_ipv4 = 1;
        int first_ipv6 = 1;
        for (PIP_ADAPTER_ADDRESSES adapter = adapter_address; adapter; adapter = adapter->Next) {
            if (adapter->TunnelType == TUNNEL_TYPE_TEREDO)
                continue;
            if (adapter->OperStatus != IfOperStatusUp)
                continue;

            for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast;
                unicast = unicast->Next) {
                if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                    struct sockaddr_in* saddr = (struct sockaddr_in*)unicast->Address.lpSockaddr;
                    if ((saddr->sin_addr.S_un.S_un_b.s_b1 != 127) ||
                        (saddr->sin_addr.S_un.S_un_b.s_b2 != 0) ||
                        (saddr->sin_addr.S_un.S_un_b.s_b3 != 0) ||
                        (saddr->sin_addr.S_un.S_un_b.s_b4 != 1)) {
                        int log_addr = 0;
                        if (first_ipv4) {
                            service_address_ipv4 = *saddr;
                            first_ipv4 = 0;
                            log_addr = 1;
                        }
                        has_ipv4 = 1;
                        if (num_sockets < max_sockets) {
                            saddr->sin_port = htons((unsigned short)port);
                            int sock = mdns_socket_open_ipv4(saddr);
                            if (sock >= 0) {
                                sockets[num_sockets++] = sock;
                                log_addr = 1;
                            } else {
                                log_addr = 0;
                            }
                        }
                        if (log_addr) {
                            char buffer[128];
                            mdns_string_t addr = ipv4_address_to_string(buffer, sizeof(buffer), saddr,
                                                                        sizeof(struct sockaddr_in));
                            mdns_log("Local IPv4 address: " + std::string(addr.str));
                        }
                    }
                } else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) {
                    struct sockaddr_in6* saddr = (struct sockaddr_in6*)unicast->Address.lpSockaddr;
                    // Ignore link-local addresses
                    if (saddr->sin6_scope_id)
                        continue;
                    static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                            0, 0, 0, 0, 0, 0, 0, 1};
                    static const unsigned char localhost_mapped[] = {0, 0, 0,    0,    0,    0, 0, 0,
                                                                    0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};
                    if ((unicast->DadState == NldsPreferred) &&
                        memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
                        memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16)) {
                        int log_addr = 0;
                        if (first_ipv6) {
                            service_address_ipv6 = *saddr;
                            first_ipv6 = 0;
                            log_addr = 1;
                        }
                        has_ipv6 = 1;
                        if (num_sockets < max_sockets) {
                            saddr->sin6_port = htons((unsigned short)port);
                            int sock = mdns_socket_open_ipv6(saddr);
                            if (sock >= 0) {
                                sockets[num_sockets++] = sock;
                                log_addr = 1;
                            } else {
                                log_addr = 0;
                            }
                        }
                        if (log_addr) {
                            char buffer[128];
                            mdns_string_t addr = ipv6_address_to_string(buffer, sizeof(buffer), saddr,
                                                                        sizeof(struct sockaddr_in6));
                            mdns_log("Local IPv6 address: " + std::string(addr.str));
                        }
                    }
                }
            }
        }

        free(adapter_address);

    #else

        struct ifaddrs* ifaddr = 0;
        struct ifaddrs* ifa = 0;

        if (getifaddrs(&ifaddr) < 0)
            mdns_log("Unable to get interface addresses");

        int first_ipv4 = 1;
        int first_ipv6 = 1;
        for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr)
                continue;
            if (!(ifa->ifa_flags & IFF_UP) || !(ifa->ifa_flags & IFF_MULTICAST))
                continue;
            if ((ifa->ifa_flags & IFF_LOOPBACK) || (ifa->ifa_flags & IFF_POINTOPOINT))
                continue;

            if (ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in* saddr = (struct sockaddr_in*)ifa->ifa_addr;
                if (saddr->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                    int log_addr = 0;
                    if (first_ipv4) {
                        service_address_ipv4 = *saddr;
                        first_ipv4 = 0;
                        log_addr = 1;
                    }
                    has_ipv4 = 1;
                    if (num_sockets < max_sockets) {
                        saddr->sin_port = htons(port);
                        int sock = mdns_socket_open_ipv4(saddr);
                        if (sock >= 0) {
                            sockets[num_sockets++] = sock;
                            log_addr = 1;
                        } else {
                            log_addr = 0;
                        }
                    }
                    if (log_addr) {
                        char buffer[128];
                        mdns_string_t addr = ipv4_address_to_string(buffer, sizeof(buffer), saddr,
                                                                    sizeof(struct sockaddr_in));
                        mdns_log("Local IPv4 address: " + std::string(addr.str));
                    }
                }
            } else if (ifa->ifa_addr->sa_family == AF_INET6) {
                struct sockaddr_in6* saddr = (struct sockaddr_in6*)ifa->ifa_addr;
                // Ignore link-local addresses
                if (saddr->sin6_scope_id)
                    continue;
                static const unsigned char localhost[] = {0, 0, 0, 0, 0, 0, 0, 0,
                                                        0, 0, 0, 0, 0, 0, 0, 1};
                static const unsigned char localhost_mapped[] = {0, 0, 0,    0,    0,    0, 0, 0,
                                                                0, 0, 0xff, 0xff, 0x7f, 0, 0, 1};
                if (memcmp(saddr->sin6_addr.s6_addr, localhost, 16) &&
                    memcmp(saddr->sin6_addr.s6_addr, localhost_mapped, 16)) {
                    int log_addr = 0;
                    if (first_ipv6) {
                        service_address_ipv6 = *saddr;
                        first_ipv6 = 0;
                        log_addr = 1;
                    }
                    has_ipv6 = 1;
                    if (num_sockets < max_sockets) {
                        saddr->sin6_port = htons(port);
                        int sock = mdns_socket_open_ipv6(saddr);
                        if (sock >= 0) {
                            sockets[num_sockets++] = sock;
                            log_addr = 1;
                        } else {
                            log_addr = 0;
                        }
                    }
                    if (log_addr) {
                        char buffer[128];
                        mdns_string_t addr = ipv6_address_to_string(buffer, sizeof(buffer), saddr,
                                                                    sizeof(struct sockaddr_in6));
                        mdns_log("Local IPv6 address: " + std::string(addr.str));
                    }
                }
            }
        }

        freeifaddrs(ifaddr);

    #endif

        return num_sockets;
    }

    // Open sockets to listen to incoming mDNS queries on port 5353
    static int open_service_sockets(int* sockets, int max_sockets) {
        // When recieving, each socket can recieve data from all network interfaces
        // Thus we only need to open one socket for each address family
        int num_sockets = 0;

        // Call the client socket function to enumerate and get local addresses,
        // but not open the actual sockets
        open_client_sockets(0, 0, 0);

        if (num_sockets < max_sockets) {
            struct sockaddr_in sock_addr;
            memset(&sock_addr, 0, sizeof(struct sockaddr_in));
            sock_addr.sin_family = AF_INET;
    #ifdef _WIN32
            sock_addr.sin_addr = in4addr_any;
    #else
            sock_addr.sin_addr.s_addr = INADDR_ANY;
    #endif
            sock_addr.sin_port = htons(MDNS_PORT);
    #ifdef __APPLE__
            sock_addr.sin_len = sizeof(struct sockaddr_in);
    #endif
            int sock = mdns_socket_open_ipv4(&sock_addr);
            if (sock >= 0)
                sockets[num_sockets++] = sock;
        }

        if (num_sockets < max_sockets) {
            struct sockaddr_in6 sock_addr;
            memset(&sock_addr, 0, sizeof(struct sockaddr_in6));
            sock_addr.sin6_family = AF_INET6;
            sock_addr.sin6_addr = in6addr_any;
            sock_addr.sin6_port = htons(MDNS_PORT);
    #ifdef __APPLE__
            sock_addr.sin6_len = sizeof(struct sockaddr_in6);
    #endif
            int sock = mdns_socket_open_ipv6(&sock_addr);
            if (sock >= 0)
                sockets[num_sockets++] = sock;
        }

        return num_sockets;
    }



    // Provide a mDNS service, answering incoming DNS-SD and mDNS queries
    int service_mdns(const char* hostname, const char* service_name, int service_port, const char* path, bool tls)
    {
        running = true;

        int sockets[32];
        int num_sockets = open_service_sockets(sockets, sizeof(sockets) / sizeof(sockets[0]));
        if (num_sockets <= 0) {
            mdns_log("Failed to open any client sockets");
            return -1;
        }
        mdns_log("Opened " + std::to_string(num_sockets) + " socket" + std::string(num_sockets ? "s" : "") + " for mDNS service");

        size_t service_name_length = strlen(service_name);
        if (!service_name_length) {
            mdns_log("Invalid service name");
            return -1;
        }

        char* service_name_buffer = (char*)malloc(service_name_length + 2);
        memcpy(service_name_buffer, service_name, service_name_length);
        if (service_name_buffer[service_name_length - 1] != '.')
            service_name_buffer[service_name_length++] = '.';
        service_name_buffer[service_name_length] = 0;
        service_name = service_name_buffer;

        mdns_log("Service mDNS: " + std::string(service_name) + ":" + std::to_string(service_port));
        mdns_log("Hostname: " + std::string(hostname));

        size_t capacity = 2048;
        void* buffer = malloc(capacity);

        mdns_string_t service_string = mdns_string_t{service_name, strlen(service_name)};
        mdns_string_t hostname_string = mdns_string_t{hostname, strlen(hostname)};

        // Build the service instance "<hostname>.<_service-name>._tcp.local." string
        char service_instance_buffer[256] = {0};
        snprintf(service_instance_buffer, sizeof(service_instance_buffer) - 1, "%.*s.%.*s",
                MDNS_STRING_FORMAT(hostname_string), MDNS_STRING_FORMAT(service_string));
        mdns_string_t service_instance_string =
            mdns_string_t{service_instance_buffer, strlen(service_instance_buffer)};

        // Build the "<hostname>.local." string
        char qualified_hostname_buffer[256] = {0};
        snprintf(qualified_hostname_buffer, sizeof(qualified_hostname_buffer) - 1, "%.*s.local.",
                MDNS_STRING_FORMAT(hostname_string));
        mdns_string_t hostname_qualified_string =
            mdns_string_t{qualified_hostname_buffer, strlen(qualified_hostname_buffer)};

        service_t service = {0};
        service.service = service_string;
        service.hostname = hostname_string;
        service.service_instance = service_instance_string;
        service.hostname_qualified = hostname_qualified_string;
        service.address_ipv4 = service_address_ipv4;
        service.address_ipv6 = service_address_ipv6;
        service.port = service_port;

        // Setup our mDNS records

        // PTR record reverse mapping "<_service-name>._tcp.local." to
        // "<hostname>.<_service-name>._tcp.local."
        mdns_record_t::mdns_record_data rd_ptr;
        rd_ptr.ptr = { service.service_instance };

        mdns_record_t ptr_rec;
        ptr_rec.name = service.service;
        ptr_rec.type = MDNS_RECORDTYPE_PTR;
        ptr_rec.data = rd_ptr;
        ptr_rec.rclass = 0;
        ptr_rec.ttl = 0;

        service.record_ptr = ptr_rec;

        // SRV record mapping "<hostname>.<_service-name>._tcp.local." to
        // "<hostname>.local." with port. Set weight & priority to 0.
        uint16_t p = service.port;
        mdns_record_t::mdns_record_data rd_srv;
        rd_srv.srv = { 0, 0, p, service.hostname_qualified};

        mdns_record_t srv_rec;
        srv_rec.name = service.service_instance;
        srv_rec.type = MDNS_RECORDTYPE_SRV;
        srv_rec.data = rd_srv;
        srv_rec.rclass = 0;
        srv_rec.ttl = 0;

        service.record_srv = srv_rec;

        // A/AAAA records mapping "<hostname>.local." to IPv4/IPv6 addresses
        mdns_record_t::mdns_record_data rd_a;
        rd_a.a = {service.address_ipv4};

        mdns_record_t a_rec;
        a_rec.name = service.hostname_qualified;
        a_rec.type = MDNS_RECORDTYPE_A;
        a_rec.data = rd_a;
        a_rec.rclass = 0;
        a_rec.ttl = 0;

        service.record_a = a_rec;


        mdns_record_t::mdns_record_data rd_aaaa;
        rd_aaaa.aaaa = {service.address_ipv6};

        mdns_record_t aaaa_rec;
        aaaa_rec.name = service.hostname_qualified;
        aaaa_rec.type = MDNS_RECORDTYPE_AAAA;
        aaaa_rec.data = rd_aaaa;
        aaaa_rec.rclass = 0;
        aaaa_rec.ttl = 0;

        service.record_aaaa = aaaa_rec;

        // Add two test TXT records for our service instance name, will be coalesced into
        // one record with both key-value pair strings by the library
        mdns_record_t::mdns_record_data rd_path;
        rd_path.txt = {{MDNS_STRING_CONST("path")},{path, std::strlen(path)}};

        mdns_record_t path_rec;
        path_rec.name = service.service_instance;
        path_rec.type = MDNS_RECORDTYPE_TXT;
        path_rec.data = rd_path;
        path_rec.rclass = 0;
        path_rec.ttl = 0;

        service.txt_record[0] = path_rec;
        
        mdns_record_t::mdns_record_data rd_tls;
        rd_tls.txt = {{MDNS_STRING_CONST("tls")},{MDNS_STRING_CONST(tls ? "1" : "0")}};
        
        mdns_record_t tls_rec;
        tls_rec.name = service.service_instance;
        tls_rec.type = MDNS_RECORDTYPE_TXT;
        tls_rec.data = rd_tls;
        tls_rec.rclass = 0;
        tls_rec.ttl = 0;

        service.txt_record[1] = tls_rec;

        // Send an announcement on startup of service
        {
            mdns_log("Sending announce");
            mdns_record_t additional[5] = {0};
            size_t additional_count = 0;
            additional[additional_count++] = service.record_srv;
            if (service.address_ipv4.sin_family == AF_INET)
                additional[additional_count++] = service.record_a;
            if (service.address_ipv6.sin6_family == AF_INET6)
                additional[additional_count++] = service.record_aaaa;
            additional[additional_count++] = service.txt_record[0];
            if(is_tls_server)
                additional[additional_count++] = service.txt_record[1];

            for (int isock = 0; isock < num_sockets; ++isock)
                mdns_announce_multicast(sockets[isock], buffer, capacity, service.record_ptr, 0, 0,
                                        additional, additional_count);
        }

        // This is a crude implementation that checks for incoming queries
        while (run_requested) {
            int nfds = 0;
            fd_set readfs;
            FD_ZERO(&readfs);
            for (int isock = 0; isock < num_sockets; ++isock) {
                if (sockets[isock] >= nfds)
                    nfds = sockets[isock] + 1;
                FD_SET(sockets[isock], &readfs);
            }

            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

            if (select(nfds, &readfs, 0, 0, &timeout) >= 0) {
                for (int isock = 0; isock < num_sockets; ++isock) {
                    if (FD_ISSET(sockets[isock], &readfs)) {
                        mdns_socket_listen(sockets[isock], buffer, capacity, service_callback,
                                        &service);
                    }
                    FD_SET(sockets[isock], &readfs);
                }
            } else {
                break;
            }
        }

        // Send a goodbye on end of service
        {
            mdns_log("Sending goodbye");
            mdns_record_t additional[5] = {0};
            size_t additional_count = 0;
            additional[additional_count++] = service.record_srv;
            if (service.address_ipv4.sin_family == AF_INET)
                additional[additional_count++] = service.record_a;
            if (service.address_ipv6.sin6_family == AF_INET6)
                additional[additional_count++] = service.record_aaaa;
            additional[additional_count++] = service.txt_record[0];
            if(is_tls_server)
                additional[additional_count++] = service.txt_record[1];

            for (int isock = 0; isock < num_sockets; ++isock)
                mdns_goodbye_multicast(sockets[isock], buffer, capacity, service.record_ptr, 0, 0,
                                        additional, additional_count);
        }

        free(buffer);
        free(service_name_buffer);

        for (int isock = 0; isock < num_sockets; ++isock)
            mdns_socket_close(sockets[isock]);
        mdns_log("Closed socket" + std::string(num_sockets ? "s" : ""));

        return 0;
    }


    void start_service(std::string hostname, std::string service, int port, std::string path, bool tls)
    {
        is_tls_server = tls;
        run_requested = true;
        service_mdns(hostname.c_str(), service.c_str(), port, path.c_str(), tls);
        running = false;
    }

    void stop_service()
    {
        run_requested = false;
    }

    bool is_running()
    {
        return running;
    }
};

} // bw::webthing