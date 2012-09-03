#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "e_connman_private.h"
#include <stdlib.h>
#include <string.h>

static const char CONNMAN_BUS_NAME[] = "net.connman";

static E_DBus_Signal_Handler *cb_name_owner_changed;
static DBusPendingCall *pending_get_name_owner;
static unsigned int init_count;
static E_DBus_Connection *conn;

EAPI int E_CONNMAN_EVENT_MANAGER_IN;
EAPI int E_CONNMAN_EVENT_MANAGER_OUT;

int _e_dbus_connman_log_dom = -1;

static inline void
_e_connman_system_name_owner_exit(void)
{
   ecore_event_add(E_CONNMAN_EVENT_MANAGER_OUT, NULL, NULL, NULL);
}

static inline void
_e_connman_system_name_owner_enter(void)
{
   ecore_event_add(E_CONNMAN_EVENT_MANAGER_IN, NULL, NULL, NULL);
}

static void
_e_connman_system_name_owner_changed(void *data __UNUSED__, DBusMessage *msg)
{
   const char *name, *from, *to;
   DBusError err;

   dbus_error_init(&err);
   if (!dbus_message_get_args(msg, &err,
                              DBUS_TYPE_STRING, &name,
                              DBUS_TYPE_STRING, &from,
                              DBUS_TYPE_STRING, &to,
                              DBUS_TYPE_INVALID))
     {
        ERR("could not get NameOwnerChanged arguments: %s: %s",
            err.name, err.message);
        dbus_error_free(&err);
        return;
     }

   if (strcmp(name, CONNMAN_BUS_NAME) != 0)
     return;

   DBG("NameOwnerChanged %s from=[%s] to=[%s]", name, from, to);

   if (from[0] == '\0' && to[0] != '\0')
     _e_connman_system_name_owner_enter();
   else if (from[0] != '\0' && to[0] == '\0')
     _e_connman_system_name_owner_exit();
   else
     ERR("unknow change from %s to %s", from, to);
}

static void
_e_connman_get_name_owner(void *data __UNUSED__, DBusMessage *msg, DBusError *err)
{
   pending_get_name_owner = NULL;
   DBG("get_name_owner msg=%p", msg);

   if (dbus_error_is_set(err))
     {
        if (!strcmp(err->name, DBUS_ERROR_NAME_HAS_NO_OWNER))
          ERR("could not get bus name owner: %s %s", err->name, err->message);
        return;
     }

   _e_connman_system_name_owner_enter();
}

/**
 * Initialize E Connection Manager (E_Connman) system.
 *
 * This will connect to ConnMan through DBus and watch for it going in and out.
 *
 * Interesting events are:
 *   - E_CONNMAN_EVENT_MANAGER_IN: issued when connman is avaiable.
 *   - E_CONNMAN_EVENT_MANAGER_OUT: issued when connman connection is lost.
 */
unsigned int
e_connman_system_init(E_DBus_Connection *edbus_conn)
{
   init_count++;

   if (init_count > 1)
      return init_count;

   _e_dbus_connman_log_dom = eina_log_domain_register("e_dbus_connman",
                                                      EINA_LOG_DEFAULT_COLOR);

   if (_e_dbus_connman_log_dom < 0)
     {
        EINA_LOG_ERR("impossible to create a log domain for edbus_connman module");
        return -1;
     }

   E_CONNMAN_EVENT_MANAGER_IN = ecore_event_type_new();
   E_CONNMAN_EVENT_MANAGER_OUT = ecore_event_type_new();

   conn = edbus_conn;
   cb_name_owner_changed = e_dbus_signal_handler_add(conn,
                        E_DBUS_FDO_BUS, E_DBUS_FDO_PATH, E_DBUS_FDO_INTERFACE,
                        "NameOwnerChanged", _e_connman_system_name_owner_changed,
                        NULL);

   pending_get_name_owner = e_dbus_get_name_owner(conn,
                                 CONNMAN_BUS_NAME, _e_connman_get_name_owner,
                                 NULL);

   return init_count;
}

/**
 * Shutdown ConnMan system
 *
 * When count drops to 0 resources will be released and no calls should be
 * made anymore.
 */
unsigned int
e_connman_system_shutdown(void)
{
   if (init_count == 0)
     {
        ERR("connman system already shut down.");
        return 0;
     }

   init_count--;
   if (init_count > 0)
      return init_count;

   if (pending_get_name_owner)
     {
        dbus_pending_call_cancel(pending_get_name_owner);
        pending_get_name_owner = NULL;
     }

   if (cb_name_owner_changed)
     {
        e_dbus_signal_handler_del(conn, cb_name_owner_changed);
        cb_name_owner_changed = NULL;
     }

   eina_log_domain_unregister(_e_dbus_connman_log_dom);
   conn = NULL;

   E_CONNMAN_EVENT_MANAGER_OUT = 0;
   E_CONNMAN_EVENT_MANAGER_IN = 0;

   return init_count;
}

