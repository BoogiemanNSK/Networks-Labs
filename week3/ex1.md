## Client-Server Model Commands



### socket()

**int socket(int domain, int type, int protocol)**

#### Description
*socket()* creates an endpoint for communication and returns a file descriptor that refers to that endpoint.  The file descriptor returned by a successful call will be the lowest-numbered file descriptor not currently open for the process.
**Non-blocking call.**

#### Return Value
On success, a file descriptor for the new socket is returned. On error, -1 is returned.

#### Errors Handling
On error *errno* is set appropriately:
* **EACCES** Permission to create a socket of the specified type and/or protocol is denied.
* **EAFNOSUPPORT** The implementation does not support the specified address family.
* **EINVAL** Unknown protocol, or protocol family not available.
* **EINVAL** Invalid flags in type.
* **EMFILE** The per-process limit on the number of open file descriptors has been reached.
* **ENFILE** The system-wide limit on the total number of open files has been reached.
* **ENOBUFS** or **ENOMEM**  Insufficient memory is available.  The socket cannot be created until sufficient resources are freed.
* **EPROTONOSUPPORT**  The protocol type or the specified protocol is not supported  within this domain.



### accept()

**int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);**

#### Description
The *accept()* system call is used with connection-based socket types. It extracts the first connection request on the queue of pending connections for the listening socket, *sockfd*, creates a new connected socket, and returns a new file descriptor referring to that socket.  The newly created socket is not in the listening state. The original socket *sockfd* is unaffected by this call.
**Blocking call, unless marked as non-blocking.**

#### Return Value
On success, these system calls return a nonnegative integer that is a file descriptor for the accepted socket.  On error, -1 is returned.

#### Errors Handling
On error *errno* is set appropriately:
* **EAGAIN** or **EWOULDBLOCK** The socket is marked nonblocking and no connections arepresent to be accepted. *POSIX.1-2001* and *POSIX.1-2008* allow either error to be returned for this case, and do not require these constants to have the same value, so a portable application should check for both possibilities.
* **EBADF** *sockfd* is not an open file descriptor.
* **ECONNABORTED**  A connection has been aborted.
* **EFAULT** The *addr* argument is not in a writable part of the user address space.
* **EINTR**  The system call was interrupted by a signal that was caught before a valid connection arrived.
* **EINVAL** Socket is not listening for connections, or addrlen is invalid (e.g., is negative).
* **EMFILE** The per-process limit on the number of open file descriptors has been reached.
* **ENFILE** The system-wide limit on the total number of open files has been reached.
* **ENOBUFS**, **ENOMEM** Not enough free memory.  This often means that the memory allocation is limited by the socket buffer limits, not by the system memory.
* **ENOTSOCK** The file descriptor *sockfd* does not refer to a socket.
* **EOPNOTSUPP** The referenced socket is not of type *SOCK_STREAM*.
* **EPROTO** Protocol error.

In addition, Linux *accept()* may fail if:
* **EPERM**  Firewall rules forbid connection.

In addition, network errors for the new socket and as defined for the protocol may be returned.  Various Linux kernels can return other errors such as **ENOSR**, **ESOCKTNOSUPPORT**, **EPROTONOSUPPORT**, **ETIMEDOUT**. The value **ERESTARTSYS** may be seen during a trace.



### select()

**int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);**

#### Description
*select()* allow a program to monitor multiple file descriptors, waiting until one or more of the file descriptors become "ready" for some class of I/O operation.  A file descriptor is considered ready if it is possible to perform a corresponding I/O operation.
**Non-blocking call.**

#### Return Value
On success, *select()* return the number of file descriptors contained in the three returned descriptor sets which may be zero if the *timeout* expires before anything interesting happens.  On error, -1 is returned, the file descriptor sets are unmodified, and *timeout* becomes undefined.

#### Errors Handling
On error *errno* is set to indicate the error:
* **EBADF**  An invalid file descriptor was given in one of the sets.
* **EINTR**  A signal was caught.
* **EINVAL** *nfds* is negative or exceeds the *RLIMIT_NOFILE* resource limit.
* **EINVAL** The value contained within *timeout* is invalid.
* **ENOMEM** Unable to allocate memory for internal tables.



### bind()

**int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);**

#### Description
When a socket is created with *socket()*, it exists in a name space but has no address assigned to it. *bind()* assigns the address specified by *addr* to the socket referred to by the file descriptor *sockfd*. *addrlen* specifies the size, in bytes, of the address structure pointed to by *addr*. Traditionally, this operation is called “assigning a name to a socket”.
**Non-blocking call.**

#### Return Value
On success, zero is returned.  On error, -1 is returned.

#### Errors Handling
On error *errno* is set appropriately:
* **EACCES** The address is protected, and the user is not the superuser.
* **EADDRINUSE** The given address is already in use.
* **EADDRINUSE** The port number was specified as zero in the socket address structure, but, upon attempting to bind to an ephemeral port, it was determined that all port numbers in the ephemeral port range are currently in use.
* **EBADF** *sockfd* is not a valid file descriptor.
* **EINVAL** The socket is already bound to an address.
* **EINVAL** *addrlen* is wrong, or *addr* is not a valid address for this socket's domain.
* **ENOTSOCK** The file descriptor *sockfd* does not refer to a socket.

The following errors are specific to UNIX domain sockets:
* **EACCES** Search permission is denied on a component of the path prefix.
* **EADDRNOTAVAIL** A nonexistent interface was requested or the requested address was not local.
* **EFAULT** *addr* points outside the user's accessible address space.
* **ELOOP**  Too many symbolic links were encountered in resolving *addr*.
* **ENAMETOOLONG** *addr* is too long.
* **ENOENT** A component in the directory prefix of the socket pathname does not exist.
* **ENOMEM** Insufficient kernel memory was available.
* **ENOTDIR** A component of the path prefix is not a directory.
* **EROFS**  The socket inode would reside on a read-only filesystem.
