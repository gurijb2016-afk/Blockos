#include "network_socket.hpp"
#include "kernel/allocator.hpp"
#include "report-problem.cpp"

// mbedTLS minimális szükséges fejlécek (Be kell linkelned az mbedtls forrást)
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"

namespace Blockos {

class HttpsClient {
private:
    mbedtls_ssl_context m_ssl;
    mbedtls_ssl_config m_conf;
    mbedtls_entropy_context m_entropy;
    mbedtls_ctr_drbg_context m_ctr_drbg;
    
    int m_socket_fd;
    bool m_tls_connected;

    // mbedTLS egyéni küldés visszahívás (összekötjük a te network_socket.cpp-ddel) [source: 1]
    static int tls_send_callback(void *ctx, const unsigned char *buf, size_t len) {
        int fd = *reinterpret_cast<int*>(ctx);
        // Meghívjuk a te POSIX/Linux rendszerhívás-emulált socket küldésedet [source: 1]
        int bytes_sent = NetworkSocket::send(fd, buf, len, 0);
        if (bytes_sent < 0) {
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }
        return bytes_sent;
    }

    // mbedTLS egyéni fogadás visszahívás
    static int tls_recv_callback(void *ctx, unsigned char *buf, size_t len) {
        int fd = *reinterpret_cast<int*>(ctx);
        int bytes_received = NetworkSocket::recv(fd, buf, len, 0);
        if (bytes_received < 0) {
            return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        }
        if (bytes_received == 0) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        return bytes_received;
    }

public:
    HttpsClient() : m_socket_fd(-1), m_tls_connected(false) {
        mbedtls_ssl_init(&m_ssl);
        mbedtls_ssl_config_init(&m_conf);
        mbedtls_ctr_drbg_init(&m_ctr_drbg);
        mbedtls_entropy_init(&m_entropy);
    }

    ~HttpsClient() {
        close();
        mbedtls_ssl_free(&m_ssl);
        mbedtls_ssl_config_free(&m_conf);
        mbedtls_ctr_drbg_free(&m_ctr_drbg);
        mbedtls_entropy_free(&m_entropy);
    }

    // Kapcsolódás HTTPS szerverhez (pl. "google.com", port: 443)
    bool connect(const char* host, uint16_t port) {
        // 1. LÉPÉS: Sima TCP Socket nyitása az lwIP-n keresztül [source: 1, 1.1.4]
        m_socket_fd = NetworkSocket::socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket_fd < 0) {
            ReportProblem(ProblemLevel::CRITICAL, "HTTPS", "Nem sikerült socketet nyitni.");
            return false;
        }

        if (!NetworkSocket::connect_by_host(m_socket_fd, host, port)) {
            ReportProblem(ProblemLevel::CRITICAL, "HTTPS", "TCP kapcsolat sikertelen.");
            return false;
        }

        // 2. LÉPÉS: Entrópia és véletlenszám-generátor inicializálása (kötelező a kriptográfiához)
        const char *pers = "blockos_https_client";
        if (mbedtls_ctr_drbg_seed(&m_ctr_drbg, mbedtls_entropy_func, &m_entropy, 
                                  reinterpret_cast<const unsigned char*>(pers), strlen(pers)) != 0) {
            return false;
        }

        // 3. LÉPÉS: SSL/TLS konfiguráció beállítása
        if (mbedtls_ssl_config_defaults(&m_conf, MBEDTLS_SSL_IS_CLIENT, 
                                        MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
            return false;
        }

        // Hobbi OS tesztkörnyezetben a tanúsítvány-ellenőrzést kikapcsoljuk (MBEDTLS_SSL_VERIFY_NONE),
        // különben be kellene töltened az összes globális Root CA tanúsítványt fájlból.
        mbedtls_ssl_conf_authmode(&m_conf, MBEDTLS_SSL_VERIFY_NONE);
        mbedtls_ssl_conf_rng(&m_conf, mbedtls_ctr_drbg_random, &m_ctr_drbg);

        if (mbedtls_ssl_setup(&m_ssl, &m_conf) != 0) {
            return false;
        }

        // SNI (Server Name Indication) beállítása - a modern szervereknél kötelező!
        mbedtls_ssl_set_hostname(&m_ssl, host);

        // Összekötjük az mbedTLS-t a te egyéni hálózati visszahívó függvényeiddel
        mbedtls_ssl_set_bio(&m_ssl, &m_socket_fd, tls_send_callback, tls_recv_callback, nullptr);

        // 4. LÉPÉS: TLS Kézfogás (Handshake) végrehajtása
        int ret;
        while ((ret = mbedtls_ssl_handshake(&m_ssl)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
                char error_buf[100];
                mbedtls_strerror(ret, error_buf, sizeof(error_buf));
                ReportProblem(ProblemLevel::CRITICAL, "HTTPS", error_buf);
                return false;
            }
            // Ha nem-blokkoló a hálózatod, itt át kell adni a futást a schedulernek [source: 1]
            Scheduler::yield(); 
        }

        m_tls_connected = true;
        ReportProblem(ProblemLevel::INFO, "HTTPS", "Sikeres biztonságos TLS kapcsolat felépült!");
        return true;
    }

    // Biztonságos adatküldés (HTTP GET kérés küldése) [source: 1.2.4]
    int send(const char* data, size_t len) {
        if (!m_tls_connected) return -1;
        return mbedtls_ssl_write(&m_ssl, reinterpret_cast<const unsigned char*>(data), len);
    }

    // Biztonságos adatfogadás (HTTP válasz beolvasása) [source: 1.2.4]
    int recv(char* buffer, size_t max_len) {
        if (!m_tls_connected) return -1;
        int ret = mbedtls_ssl_read(&m_ssl, reinterpret_cast<unsigned char*>(buffer), max_len);
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            return 0; // Még nincs adat, próbálja újra később
        }
        return ret; // Visszaadja a beolvasott bájtok számát
    }

    void close() {
        if (m_tls_connected) {
            mbedtls_ssl_close_notify(&m_ssl);
            m_tls_connected = false;
        }
        if (m_socket_fd >= 0) {
            NetworkSocket::close(m_socket_fd);
            m_socket_fd = -1;
        }
    }
};

// Példa a modul használatára egy felhasználói programból vagy a shell.cpp-ből [source: 1]
void perform_https_request() {
    HttpsClient client;
    if (client.connect("api.github.com", 443)) {
        const char* req = "GET /repos/yhirose/cpp-httplib HTTP/1.1\r\n"
                          "Host: api.github.com\r\n"
                          "User-Agent: BlockosOS_HTTPS_Client\r\n"
                          "Connection: close\r\n\r\n";
        
        client.send(req, strlen(req));

        char buf[512];
        int bytes;
        while ((bytes = client.recv(buf, sizeof(buf) - 1)) >= 0) {
            if (bytes > 0) {
                buf[bytes] = '\0';
                // Kiírjuk a letöltött adatot a képernyőre vagy a logba
                ReportProblem(ProblemLevel::INFO, "HTTPS_DATA", buf);
            } else {
                Scheduler::yield(); // Várakozás a következő hálózati csomagra [source: 1]
            }
        }
    }
}

} // namespace Blockos
