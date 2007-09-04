#ifndef __RTS2_BLOCK__
#define __RTS2_BLOCK__

/**
 * @file
 * Holds base Rts2Block class. This class is common ancestor of RTS2 devices, daemons and clients.
 *
 * @defgroup RTS2Block
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include <list>
#include "status.h"

#include <sstream>

#include "rts2event.h"
#include "rts2object.h"
#include "rts2conn.h"
#include "rts2option.h"
#include "rts2address.h"
#include "rts2user.h"
#include "rts2devclient.h"
#include "rts2value.h"
#include "rts2valuestat.h"
#include "rts2valueminmax.h"
#include "rts2app.h"
#include "rts2serverstate.h"

// protocol specific commands
#define PROTO_VALUE		"V"
#define PROTO_SET_VALUE		"X"
#define PROTO_SET_VALUE_DEF	"Y"
#define PROTO_DATA		"D"
#define PROTO_AUTH		"A"
#define PROTO_PRIORITY		"P"
#define PROTO_PRIORITY_INFO	"Q"
#define PROTO_INFO		"I"
#define PROTO_STATUS		"S"
#define PROTO_TECHNICAL		"T"
#define PROTO_MESSAGE		"M"
#define PROTO_METAINFO		"E"
#define PROTO_SELMETAINFO       "F"

#define USEC_SEC		1000000

class Rts2Command;

class Rts2ClientTCPDataConn;

class Rts2DevClient;

class Rts2LogStream;

/**
 * Hold list of connections. It is used to store \see Rts2Conn objects.
 */
typedef
  std::list <
Rts2Conn * >
  connections_t;

/**
 * Base class of RTS2 devices and clients. Contain RTS2 related management functions - manage list of connections, and basic commands
 * which are passed on conditions (e.g. status messages).
 *
 * @ingroup RTS2Block
 */
class
  Rts2Block:
  public
  Rts2App
{
private:
  int
    port;
  long int
    idle_timeout;		// in msec
  int
    priority_client;

  connections_t
    connections;

  std::list <
  Rts2Address * >
    blockAddress;
  std::list <
  Rts2User * >
    blockUsers;

  int
    masterState;

protected:

  virtual
    Rts2Conn *
  createClientConnection (char *in_deviceName) = 0;
  virtual Rts2Conn *
  createClientConnection (Rts2Address * in_addr) = 0;

  virtual void
  cancelPriorityOperations ();

  virtual void
  childReturned (pid_t child_pid);
  virtual int
  willConnect (Rts2Address * in_addr);	// determine if the device wants to connect to recently added device; returns 0 if we won't connect, 1 if we will connect

  /***
   * Address list related functions.
   **/
  virtual int
  addAddress (Rts2Address * in_addr);
  virtual void
  addSelectSocks (fd_set * read_set);
  virtual void
  selectSuccess (fd_set * read_set);
  void
  setMessageMask (int new_mask);

  /**
   * Called before connection is deleted from connection list.
   * This hook method can cause connection to not be deleted by returning
   * non-zero value.
   *
   * @param conn This connection is marked for deletion.
   * @return 0 when connection can be deleted, non-zero when some error is
   *    detected and connection should be keeped in list of active connections.
   *
   * @post conn is removed from the list, @see Rts2Block::connectionRemoved is
   * called, and conn is deleted.
   */
  virtual int
  deleteConnection (Rts2Conn * conn);

  /**
   * Called when connection is removed from connection list, but before connection object is deleted.
   *
   * @param conn Connection which is removed from connection list, and will be deleted after this command returns.
   *
   * @pre conn is removed from connection list.
   * @post conn instance is deleted.
   */
  virtual void
  connectionRemoved (Rts2Conn * conn);

public:

  /**
   * Basic constructor. Fill argc and argv values.
   *
   * @param in_argc Number of agruments, ussually argc passed from main call.
   * @param in_argv Block arguments, ussually passed from main call.
   */
  Rts2Block (int in_argc, char **in_argv);

  /**
   * Delete list of conncection, clear Rts2Block structure.
   */
  virtual ~
  Rts2Block (void);

  /**
   * Set port number of listening socket.
   *
   * Rts2Block ussually create listening socket, so other RTS2 programs can connect to the component.
   * 
   * @param in_port Port number. Usually RTS2 blocks will use ports above 1020.
   */
  void
  setPort (int in_port);

  /**
   * Return listening port number.
   *
   * @return Listening port number, -1 when listening port is not opened.
   */
  int
  getPort (void);

  /**
   * Add connection to given block.
   *
   * @param conn Connection which will be added to connections of the block.
   */
  void
  addConnection (Rts2Conn * conn);

  /**
   * Returns begin iterator of connections structure.
   *
   * @return connections.begin() iterator.
   */
  connections_t::iterator
  connectionBegin ()
  {
    return connections.begin ();
  }

  /**
   * Returns end iterator of connections structure.
   *
   * @see Rts2Block::connectionBegin
   *
   * @return connections.end() iterator.
   */
  connections_t::iterator
  connectionEnd ()
  {
    return connections.end ();
  }

  /**
   * Return connection at given number.
   *
   * @param i Number of connection which will be returned.
   *
   * @return NULL if connection with given number does not exists, or @see Rts2Conn reference if it does.
   *
   * @bug since connections_t is list, [] operator cannot be used. vector caused some funny problems.
   */
  Rts2Conn *
  connectionAt (int i)
  {
    int
      j;
    connections_t::iterator iter;
    for (j = 0, iter = connections.begin ();
	 j < i && iter != connections.end (); j++, iter++);
    if (iter == connections.end ())
      return NULL;
    return *iter;
  }

  /**
   * Return number of connections in connections structure.
   *
   * @return Number of connections in block.
   */
  int
  connectionSize ()
  {
    return connections.size ();
  }

  /**
   * Ask if command que is empty.
   *
   * If command is running (e.g. was send to the conection, but Rts2Block does
   * not received reply), it will return True.
   *
   * @return True if command que is empty and new command will be executed
   * immediately (after running command returns), otherwise returns false.
   */
  bool
  commandQueEmpty ();

  /**
   * Event handling mechanism.
   *
   * Send Event to all connections which are members of Rts2Block structure.
   *
   * @see Rts2Event
   * @see Rts2Object::postEvent
   *
   * @param event Event which is passed to postEvent method.
   */
  virtual void
  postEvent (Rts2Event * event);

  /**
   * Used to create new connection - so childrens can
   * create childrens of Rts2Conn
   */
  virtual Rts2Conn *
  createConnection (int in_sock);

  /**
   * Create data connection. Various parameters determine connection which
   * requeired data connection, data connection originator, 
   *
   *
   * @see Rts2ClientTCPDataConn
   *
   * @param in_conn
   * @param in_hostname
   * @param in_port
   * @param in_size
   *
   * @return Rts2ClientTCPDataConn instance, which represent newly created data
   * connection.
   */
  Rts2Conn *
  addDataConnection (Rts2Conn * in_conn, char *in_hostname,
		     int in_port, int in_size);


  Rts2Conn *
  findName (const char *in_name);
  Rts2Conn *
  findCentralId (int in_id);
  int
  sendStatusMessage (int state);
  int
  sendStatusMessage (int state, Rts2Conn * conn);
  int
  sendAll (char *msg);
  void
  sendValueAll (char *val_name, char *value);
  // don't escape string..
  void
  sendValueRawAll (char *val_name, char *value);
  int
  sendPriorityChange (int p_client, int timeout);
  // only used in centrald!
  void
  sendMessageAll (Rts2Message & msg);
  virtual int
  idle ();
  void
  setTimeout (long int new_timeout)
  {
    idle_timeout = new_timeout;
  }
  void
  setTimeoutMin (long int new_timeout)
  {
    if (new_timeout < idle_timeout)
      idle_timeout = new_timeout;
  }
  void
  oneRunLoop ();

  int
  setPriorityClient (int in_priority_client, int timeout);
  void
  checkPriority (Rts2Conn * conn)
  {
    if (conn->getCentraldId () == priority_client)
      {
	conn->setHavePriority (1);
      }
  }

  /**
   * This function is called when device on given connection is ready
   * to accept commands.
   *
   * \param conn connection representing device which became ready
   */
  virtual void
  deviceReady (Rts2Conn * conn);

  /**
   * Called when some device connected to us become idle.
   *
   * \param conn connection representing device which became idle
   */
  virtual void
  deviceIdle (Rts2Conn * conn);

  virtual int
  changeMasterState (int new_state)
  {
    return 0;
  }
  int
  setMasterState (int new_state)
  {
    masterState = new_state;
    return changeMasterState (new_state);
  }
  int
  getMasterState ()
  {
    return masterState;
  }
  Rts2Address *
  findAddress (const char *blockName);

  void
  addAddress (const char *p_name, const char *p_host, int p_port,
	      int p_device_type);

  void
  deleteAddress (const char *p_name);

  virtual Rts2DevClient *
  createOtherType (Rts2Conn * conn, int other_device_type);
  void
  addUser (int p_centraldId, int p_priority, char p_priority_have,
	   const char *p_login);
  int
  addUser (Rts2User * in_user);

  /***************************************************************
   * 
   * Return established connection to device with given name.
   *
   * Returns connection to device with deviceName. Device must be know to system.
   *
   ***************************************************************/

  Rts2Conn *
  getOpenConnection (const char *deviceName);

  /***************************************************************
   *
   * Return connection to given device.
   * 
   * Create and return new connection if if device name isn't found
   * among connections, but is in address list.
   *
   * Cann return 'fake' client connection, which will not resolve 
   * to device name (even after 'info' call on master device).
   * For every command enqued to fake devices error handler will be
   * runned.
   *
   ***************************************************************/
  Rts2Conn *
  getConnection (char *deviceName);

  virtual Rts2Conn *
  getCentraldConn ()
  {
    return NULL;
  }

  virtual void
  message (Rts2Message & msg);

  int
  queAll (Rts2Command * cmd);
  int
  queAll (char *text);

  int
  allQuesEmpty ();

  // enables to grant priority for special device links
  virtual int
  grantPriority (Rts2Conn * conn)
  {
    return 0;
  }

  /** 
   * Return connection with minimum (integer) value.
   */
  Rts2Conn *
  getMinConn (const char *valueName);

  virtual void
  centraldConnRunning ()
  {
  }

  virtual void
  centraldConnBroken ()
  {
  }

  virtual int
  setValue (Rts2Conn * conn, bool overwriteSaved)
  {
    return -2;
  }

  Rts2Value *
  getValue (const char *device_name, const char *value_name);

  virtual void
  endRunLoop ()
  {
    setEndLoop (true);
  }

  double
  getNow ()
  {
    struct timeval
      infot;
    gettimeofday (&infot, NULL);
    return infot.tv_sec + (double) infot.tv_usec / USEC_SEC;
  }

  virtual int
  statusInfo (Rts2Conn * conn);

  bool
  commandPending (Rts2Command * cmd);
};

#endif /*! __RTS2_NETBLOCK__ */
