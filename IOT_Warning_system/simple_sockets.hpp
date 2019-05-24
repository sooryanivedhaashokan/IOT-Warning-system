#ifndef __SIMPLE_SOCKETS_HPP__
#define __SIMPLE_SOCKETS_HPP__

#include <string>
#include <vector>

/**
 * Simple abstraction for a socket that makes it easier to setup
 * the socket and send and receive messages.
 */
class ClientSocket {
  private:
    //Remember the port and ip address for later logging
    uint32_t _port;
    std::string _ip_address;

    //Hide the socket to make sure it isn't misused
    int sock_fd;

    //No copying or assignment. Deleting the copy constructor prevents passing
    //by value. This makes sure the socket is used in one place and deleted
    //only once.
    ClientSocket& operator=(const ClientSocket&) = delete;
    ClientSocket(const ClientSocket&) = delete;
  public:
    /**
     * The domain - generally either AF_INET of AF_INET6, can be AF_UNSPEC for either.
     * Type is SOCK_STREAM for TCP and SOCK_DGRAM for UDP (the most common)
     * Protocol is generally 0 to use the default protocol.
     */
    ClientSocket(int domain, int type, int protocol, uint32_t port, const std::string& ip_address, int sock_flags = 0);

    ///Constructor to take in a preexisting socket
    ClientSocket(uint32_t port, const std::string& ip_address, int sock);

    ///Destructor - close the socket if it is open
    ~ClientSocket();

    ///Evalute to true if socket is open, false otherwise
    explicit operator bool() const;

    /**
     * Allow copy form an rvalue
     * Self-copy cannot happen because an r-value reference cannot be
     * created and assigned to itself (since it is temporary in some scope)
     */
    ClientSocket& operator=(ClientSocket&& other);

    /**
     * Allow copy from an rvalue as with the assignment operator.
     */
    ClientSocket(ClientSocket&& other);

    ssize_t receive(std::vector<unsigned char>& buff);
    void send(const std::vector<unsigned char>& buff);

    uint32_t port();
    std::string ip_address();
};


/**
 * Simple abstraction of a socket used to receive new incoming connections.
 */
struct ServerSocket {
  private:
    uint32_t _port;
    int sock_fd;

    //No copying or assignment. Deleting the copy constructor prevents passing
    //by value. This makes sure the socket is used in one place and deleted
    //only once.
    ServerSocket& operator=(const ClientSocket&) = delete;
    ServerSocket(const ClientSocket&) = delete;

  public:
    /**
     * Set up the socket
     * @domain - generally either AF_INET of AF_INET6, can be AF_UNSPEC for either.
     * @type - SOCK_STREAM for TCP and SOCK_DGRAM for UDP
     * @sock_flags - flags that be will bitwise ORed with the type when creating the socket
     */
    ServerSocket(int domain, int type, int sock_flags, uint32_t port);

    ///Close the socket in the destructor
    ~ServerSocket();

    ///Evalute to true the socket is open, false otherwise
    explicit operator bool() const;

    //Accept a new socket connection and return a ClientSocket to talk through.
    ClientSocket next(int flags = 0);
};

#endif

