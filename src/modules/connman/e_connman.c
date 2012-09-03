#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "E_Connman.h"

#define CONNMAN_BUS_NAME "net.connman"
#define CONNMAN_MANAGER_IFACE CONNMAN_BUS_NAME ".Manager"

enum Connman_State
{
   CONNMAN_STATE_NONE = -1, /* All unknown states */
   CONNMAN_STATE_OFFLINE,
   CONNMAN_STATE_IDLE,
   CONNMAN_STATE_READY,
   CONNMAN_STATE_ONLINE,
};

struct Connman_Object
{
   const char *path;
   Eina_List *handlers; /* E_DBus_Signal_Handler */
};

struct Connman_Manager
{
   struct Connman_Object obj;

   Eina_Inlist *services; /* The prioritized list of services */

   /* Properties */
   enum Connman_State state;
   bool offline_mode;

   /* Private */
   struct
     {
        DBusPendingCall *get_services;
        DBusPendingCall *get_properties;
     } pending;
};

static unsigned int init_count;
static E_DBus_Connection *conn;
static char *bus_owner;
static struct Connman_Manager *connman_manager;

static DBusPendingCall *pending_get_name_owner;
static E_DBus_Signal_Handler *handler_name_owner;

EAPI int E_CONNMAN_EVENT_MANAGER_IN;
EAPI int E_CONNMAN_EVENT_MANAGER_OUT;

/* utility functions */

static bool _dbus_bool_get(DBusMessageIter *itr)
{
   dbus_bool_t val;
   dbus_message_iter_get_basic(itr, &val);
   return val;
}

static enum Connman_State str_to_state(const char *s)
{
   if (strcmp(s, "offline") == 0)
     return CONNMAN_STATE_OFFLINE;
   if (strcmp(s, "idle") == 0)
     return CONNMAN_STATE_IDLE;
   if (strcmp(s, "ready") == 0)
     return CONNMAN_STATE_READY;
   if (strcmp(s, "online") == 0)
     return CONNMAN_STATE_ONLINE;

   ERR("Unknown state %s", s);
   return CONNMAN_STATE_NONE;
}

/* ---- */

static void _connman_object_init(struct Connman_Object *obj, const char *path)
{
   EINA_SAFETY_ON_NULL_RETURN(path);
   obj->path = path;
}

static void _service_changed(const char *service, DBusMessageIter *props)
{
}

static void _manager_services_changed(void *data, DBusMessage *msg)
{
}

static void _manager_get_services_cb(void *data, DBusMessage *reply,
                                     DBusError *err)
{
}

static void _manager_parse_prop_changed(struct Connman_Manager *cm,
                                        const char *name,
                                        DBusMessageIter *value)
{
   if (strcmp(name, "State") == 0)
     {
        const char *state;
        dbus_message_iter_get_basic(value, &state);
        DBG("New state: %s", state);
        cm->state = str_to_state(state);
     }
   else if (strcmp(name, "OfflineMode") == 0)
     cm->offline_mode = _dbus_bool_get(value);
   else
     DBG("Unhandled property '%s'", name);
}

static void _manager_prop_changed(void *data, DBusMessage *msg)
{
   struct Connman_Manager *cm = data;
   DBusMessageIter iter, var;
   const char *name;

   if (!msg || !dbus_message_iter_init(msg, &iter))
     {
        ERR("Could not parse message %p", msg);
        return;
     }

   dbus_message_iter_get_basic(&iter, &name);
   dbus_message_iter_next(&iter);
   dbus_message_iter_recurse(&iter, &var);

   _manager_parse_prop_changed(cm, name, &var);
}

static void _manager_get_prop_cb(void *data, DBusMessage *reply,
                                 DBusError *err)
{
   struct Connman_Manager *cm = data;
   DBusMessageIter iter, dict;

   cm->pending.get_properties = NULL;

   if (dbus_error_is_set(err))
     {
        DBG("Could not get properties. %s: %s", err->name, err->message);
        return;
     }

   dbus_message_iter_init(reply, &iter);
   dbus_message_iter_recurse(&iter, &dict);

   for (; dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID;
        dbus_message_iter_next(&dict))
     {
        DBusMessageIter entry, var;
        const char *name;

        dbus_message_iter_recurse(&dict, &entry);
        dbus_message_iter_get_basic(&entry, &name);
        dbus_message_iter_next(&entry);
        dbus_message_iter_recurse(&entry, &var);

        _manager_parse_prop_changed(cm, name, &var);
     }
}

static void _manager_free(struct Connman_Manager *cm)
{
   Eina_List *l;

   EINA_LIST_FREE(cm->obj.handlers, l)
     e_dbus_signal_handler_del(conn, l->data);

   if (cm->pending.get_services)
     {
        dbus_pending_call_cancel(cm->pending.get_services);
        cm->pending.get_services = NULL;
     }

   if (cm->pending.get_properties)
     {
        dbus_pending_call_cancel(cm->pending.get_properties);
        cm->pending.get_properties = NULL;
     }
}

static struct Connman_Manager *_manager_new(void)
{
   DBusMessage *msg_props, *msg_services;
   const char *path = "/";
   struct E_DBus_Signal_Handler *h;
   struct Connman_Manager *cm;

   msg_services = dbus_message_new_method_call(CONNMAN_BUS_NAME, "/",
                                      CONNMAN_MANAGER_IFACE, "GetServices");
   msg_props = dbus_message_new_method_call(CONNMAN_BUS_NAME, "/",
                                      CONNMAN_MANAGER_IFACE, "GetProperties");

   if (!msg_services || !msg_services)
     {
        ERR("Could not create D-Bus messages");
        return NULL;
     }

   cm = calloc(1, sizeof(*cm) + 2);
   EINA_SAFETY_ON_NULL_RETURN_VAL(cm, NULL);

   memcpy(cm + sizeof(*cm), path, 1);
   _connman_object_init(&cm->obj, path);

   h = e_dbus_signal_handler_add(conn, bus_owner,
                                 path, CONNMAN_MANAGER_IFACE, "PropertyChanged",
                                 _manager_prop_changed, cm);
   cm->obj.handlers =  eina_list_append(cm->obj.handlers, h);

   h = e_dbus_signal_handler_add(conn, bus_owner,
                                 path, CONNMAN_MANAGER_IFACE, "ServicesChanged",
                                 _manager_services_changed, cm);
   cm->obj.handlers =  eina_list_append(cm->obj.handlers, h);

   /*
    * PropertyChanged signal in service's path is guaranteed to arrive only
    * after ServicesChanged above. So we only add the handler later, in a per
    * service manner.
    */

   cm->pending.get_services = e_dbus_message_send(conn, msg_services,
                                             _manager_get_services_cb, -1, cm);
   cm->pending.get_properties = e_dbus_message_send(conn, msg_props,
                                             _manager_get_prop_cb, -1, cm);

   return cm;
}

static inline void _e_connman_system_name_owner_exit(void)
{
   free(bus_owner);
   bus_owner = NULL;

   _manager_free(connman_manager);
   connman_manager = NULL;

   ecore_event_add(E_CONNMAN_EVENT_MANAGER_OUT, NULL, NULL, NULL);
}

static inline void _e_connman_system_name_owner_enter(const char *owner)
{
   bus_owner = strdup(owner);
   connman_manager = _manager_new();
   ecore_event_add(E_CONNMAN_EVENT_MANAGER_IN, NULL, NULL, NULL);
}

static void _e_connman_system_name_owner_changed(void *data __UNUSED__,
                                                 DBusMessage *msg)
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
     _e_connman_system_name_owner_enter(to);
   else if (from[0] != '\0' && to[0] == '\0')
     _e_connman_system_name_owner_exit();
   else
     ERR("unknow change from %s to %s", from, to);
}

static void
_e_connman_get_name_owner(void *data __UNUSED__, DBusMessage *msg, DBusError *err)
{
   const char *owner;

   pending_get_name_owner = NULL;

   /* Do nothing if already received a signal */
   if (bus_owner)
     return;

   DBG("get_name_owner msg=%p", msg);

   if (dbus_error_is_set(err))
     {
        if (!strcmp(err->name, DBUS_ERROR_NAME_HAS_NO_OWNER))
          ERR("could not get bus name owner: %s %s", err->name, err->message);
        return;
     }

   if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &owner,
                              DBUS_TYPE_INVALID))
     {
        ERR("Could not get name owner");
        return;
     }

   _e_connman_system_name_owner_enter(owner);
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

   E_CONNMAN_EVENT_MANAGER_IN = ecore_event_type_new();
   E_CONNMAN_EVENT_MANAGER_OUT = ecore_event_type_new();

   conn = edbus_conn;
   handler_name_owner = e_dbus_signal_handler_add(conn,
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

   e_dbus_signal_handler_del(conn, handler_name_owner);
   if (pending_get_name_owner)
     dbus_pending_call_cancel(pending_get_name_owner);

   conn = NULL;

   E_CONNMAN_EVENT_MANAGER_OUT = 0;
   E_CONNMAN_EVENT_MANAGER_IN = 0;

   return init_count;
}
