#include <dbus/dbus.h>

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLUEZ_BUS_NAME "org.bluez"
#define BLUEZ_ADAPTER_PATH "/org/bluez/hci0"
#define BLUEZ_ADV_MGR_IFACE "org.bluez.LEAdvertisingManager1"
#define BLUEZ_ADV_IFACE "org.bluez.LEAdvertisement1"
#define DBUS_OBJECT_MANAGER_IFACE "org.freedesktop.DBus.ObjectManager"
#define DBUS_PROPERTIES_IFACE "org.freedesktop.DBus.Properties"
#define DBUS_INTROSPECTABLE_IFACE "org.freedesktop.DBus.Introspectable"
#define DBUS_PEER_IFACE "org.freedesktop.DBus.Peer"
#define ADV_OBJECT_PATH "/com/parkia/netapp/advertisement0"
#define BLE_SERVICE_UUID "00001234-0000-1000-8000-00805F9B34FB"
#define BLE_APPEARANCE 0x0080

static volatile sig_atomic_t s_running = 1;
static DBusConnection *s_connection;
static bool s_registered;
static char s_local_name[64] = "SmartOne";

static void on_signal(int signo)
{
	(void)signo;
	s_running = 0;
}

static void install_signal_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = on_signal;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
}

static DBusHandlerResult send_error(DBusConnection *conn, DBusMessage *msg,
				    const char *name, const char *text)
{
	DBusMessage *reply;

	reply = dbus_message_new_error(msg, name, text);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static dbus_bool_t append_string_variant(DBusMessageIter *dict,
					 const char *key,
					 const char *value)
{
	DBusMessageIter entry;
	DBusMessageIter variant;
	const char *str = value;

	if (!dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL,
					      &entry))
		return FALSE;
	if (!dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key))
		return FALSE;
	if (!dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s",
					      &variant))
		return FALSE;
	if (!dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &str))
		return FALSE;
	if (!dbus_message_iter_close_container(&entry, &variant))
		return FALSE;
	if (!dbus_message_iter_close_container(dict, &entry))
		return FALSE;
	return TRUE;
}

static dbus_bool_t append_bool_variant(DBusMessageIter *dict, const char *key,
				       dbus_bool_t value)
{
	DBusMessageIter entry;
	DBusMessageIter variant;
	dbus_bool_t flag = value;

	if (!dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL,
					      &entry))
		return FALSE;
	if (!dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key))
		return FALSE;
	if (!dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "b",
					      &variant))
		return FALSE;
	if (!dbus_message_iter_append_basic(&variant, DBUS_TYPE_BOOLEAN, &flag))
		return FALSE;
	if (!dbus_message_iter_close_container(&entry, &variant))
		return FALSE;
	if (!dbus_message_iter_close_container(dict, &entry))
		return FALSE;
	return TRUE;
}

static dbus_bool_t append_uint16_variant(DBusMessageIter *dict, const char *key,
					 uint16_t value)
{
	DBusMessageIter entry;
	DBusMessageIter variant;
	dbus_uint16_t number = value;

	if (!dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL,
					      &entry))
		return FALSE;
	if (!dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key))
		return FALSE;
	if (!dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "q",
					      &variant))
		return FALSE;
	if (!dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT16, &number))
		return FALSE;
	if (!dbus_message_iter_close_container(&entry, &variant))
		return FALSE;
	if (!dbus_message_iter_close_container(dict, &entry))
		return FALSE;
	return TRUE;
}

static dbus_bool_t append_string_array_variant(DBusMessageIter *dict,
					       const char *key,
					       const char *value)
{
	DBusMessageIter entry;
	DBusMessageIter variant;
	DBusMessageIter array;
	const char *str = value;

	if (!dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY, NULL,
					      &entry))
		return FALSE;
	if (!dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key))
		return FALSE;
	if (!dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "as",
					      &variant))
		return FALSE;
	if (!dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY, "s",
					      &array))
		return FALSE;
	if (!dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &str))
		return FALSE;
	if (!dbus_message_iter_close_container(&variant, &array))
		return FALSE;
	if (!dbus_message_iter_close_container(&entry, &variant))
		return FALSE;
	if (!dbus_message_iter_close_container(dict, &entry))
		return FALSE;
	return TRUE;
}

static dbus_bool_t append_property_value(DBusMessageIter *iter,
					 const char *property)
{
	if (strcmp(property, "Type") == 0)
		return append_string_variant(iter, "Type", "peripheral");
	if (strcmp(property, "ServiceUUIDs") == 0)
		return append_string_array_variant(iter, "ServiceUUIDs",
						 BLE_SERVICE_UUID);
	if (strcmp(property, "LocalName") == 0)
		return append_string_variant(iter, "LocalName", s_local_name);
	if (strcmp(property, "Appearance") == 0)
		return append_uint16_variant(iter, "Appearance",
					     BLE_APPEARANCE);
	if (strcmp(property, "Discoverable") == 0)
		return append_bool_variant(iter, "Discoverable", TRUE);

	return FALSE;
}

static DBusHandlerResult handle_get_all(DBusConnection *conn, DBusMessage *msg)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *iface = NULL;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &iface,
				   DBUS_TYPE_INVALID))
		return send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
				 "missing interface argument");

	if (strcmp(iface, BLUEZ_ADV_IFACE) != 0)
		return send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
				 "unsupported interface");

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_iter_init_append(reply, &iter);
	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}",
					      &dict) ||
	    !append_property_value(&dict, "Type") ||
	    !append_property_value(&dict, "ServiceUUIDs") ||
	    !append_property_value(&dict, "LocalName") ||
	    !append_property_value(&dict, "Appearance") ||
	    !append_property_value(&dict, "Discoverable") ||
	    !dbus_message_iter_close_container(&iter, &dict)) {
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_get_managed_objects(DBusConnection *conn,
						    DBusMessage *msg)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	DBusMessageIter objects;
	DBusMessageIter object_entry;
	DBusMessageIter ifaces;
	DBusMessageIter iface_entry;
	DBusMessageIter properties;
	const char *path = ADV_OBJECT_PATH;
	const char *iface = BLUEZ_ADV_IFACE;

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_iter_init_append(reply, &iter);
	if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY,
					      "{oa{sa{sv}}}", &objects) ||
	    !dbus_message_iter_open_container(&objects, DBUS_TYPE_DICT_ENTRY,
					      NULL, &object_entry) ||
	    !dbus_message_iter_append_basic(&object_entry,
					    DBUS_TYPE_OBJECT_PATH, &path) ||
	    !dbus_message_iter_open_container(&object_entry, DBUS_TYPE_ARRAY,
					      "{sa{sv}}", &ifaces) ||
	    !dbus_message_iter_open_container(&ifaces, DBUS_TYPE_DICT_ENTRY,
					      NULL, &iface_entry) ||
	    !dbus_message_iter_append_basic(&iface_entry, DBUS_TYPE_STRING,
					    &iface) ||
	    !dbus_message_iter_open_container(&iface_entry, DBUS_TYPE_ARRAY,
					      "{sv}", &properties) ||
	    !append_property_value(&properties, "Type") ||
	    !append_property_value(&properties, "ServiceUUIDs") ||
	    !append_property_value(&properties, "LocalName") ||
	    !append_property_value(&properties, "Appearance") ||
	    !append_property_value(&properties, "Discoverable") ||
	    !dbus_message_iter_close_container(&iface_entry, &properties) ||
	    !dbus_message_iter_close_container(&ifaces, &iface_entry) ||
	    !dbus_message_iter_close_container(&object_entry, &ifaces) ||
	    !dbus_message_iter_close_container(&objects, &object_entry) ||
	    !dbus_message_iter_close_container(&iter, &objects)) {
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_NEED_MEMORY;
	}

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_get(DBusConnection *conn, DBusMessage *msg)
{
	DBusMessage *reply;
	DBusMessageIter iter;
	const char *iface = NULL;
	const char *property = NULL;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &iface,
				   DBUS_TYPE_STRING, &property,
				   DBUS_TYPE_INVALID))
		return send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
				 "missing property arguments");

	if (strcmp(iface, BLUEZ_ADV_IFACE) != 0)
		return send_error(conn, msg, DBUS_ERROR_INVALID_ARGS,
				 "unsupported interface");

	reply = dbus_message_new_method_return(msg);
	if (!reply)
		return DBUS_HANDLER_RESULT_NEED_MEMORY;

	dbus_message_iter_init_append(reply, &iter);
	if (strcmp(property, "Type") == 0) {
		DBusMessageIter variant;
		const char *value = "peripheral";

		if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
						      "s", &variant) ||
		    !dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING,
						    &value) ||
		    !dbus_message_iter_close_container(&iter, &variant)) {
			dbus_message_unref(reply);
			return DBUS_HANDLER_RESULT_NEED_MEMORY;
		}
	} else if (strcmp(property, "ServiceUUIDs") == 0) {
		DBusMessageIter variant;
		DBusMessageIter array;
		const char *value = BLE_SERVICE_UUID;

		if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
						      "as", &variant) ||
		    !dbus_message_iter_open_container(&variant, DBUS_TYPE_ARRAY,
						      "s", &array) ||
		    !dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING,
						    &value) ||
		    !dbus_message_iter_close_container(&variant, &array) ||
		    !dbus_message_iter_close_container(&iter, &variant)) {
			dbus_message_unref(reply);
			return DBUS_HANDLER_RESULT_NEED_MEMORY;
		}
	} else if (strcmp(property, "LocalName") == 0) {
		DBusMessageIter variant;
		const char *value = s_local_name;

		if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
						      "s", &variant) ||
		    !dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING,
						    &value) ||
		    !dbus_message_iter_close_container(&iter, &variant)) {
			dbus_message_unref(reply);
			return DBUS_HANDLER_RESULT_NEED_MEMORY;
		}
	} else if (strcmp(property, "Appearance") == 0) {
		DBusMessageIter variant;
		dbus_uint16_t value = BLE_APPEARANCE;

		if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
						      "q", &variant) ||
		    !dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT16,
						    &value) ||
		    !dbus_message_iter_close_container(&iter, &variant)) {
			dbus_message_unref(reply);
			return DBUS_HANDLER_RESULT_NEED_MEMORY;
		}
	} else if (strcmp(property, "Discoverable") == 0) {
		DBusMessageIter variant;
		dbus_bool_t value = TRUE;

		if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT,
						      "b", &variant) ||
		    !dbus_message_iter_append_basic(&variant,
						    DBUS_TYPE_BOOLEAN,
						    &value) ||
		    !dbus_message_iter_close_container(&iter, &variant)) {
			dbus_message_unref(reply);
			return DBUS_HANDLER_RESULT_NEED_MEMORY;
		}
	} else {
		dbus_message_unref(reply);
		return send_error(conn, msg, DBUS_ERROR_UNKNOWN_PROPERTY,
				 "unknown property");
	}

	dbus_connection_send(conn, reply, NULL);
	dbus_message_unref(reply);
	return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult advertisement_message_handler(DBusConnection *conn,
						       DBusMessage *msg,
						       void *user_data)
{
	static const char introspection_xml[] =
		"<node>"
		"  <interface name='org.freedesktop.DBus.ObjectManager'>"
		"    <method name='GetManagedObjects'>"
		"      <arg name='objects' type='a{oa{sa{sv}}}' direction='out'/>"
		"    </method>"
		"  </interface>"
		"  <interface name='org.freedesktop.DBus.Introspectable'>"
		"    <method name='Introspect'>"
		"      <arg name='xml' type='s' direction='out'/>"
		"    </method>"
		"  </interface>"
		"  <interface name='org.freedesktop.DBus.Properties'>"
		"    <method name='Get'>"
		"      <arg name='interface' type='s' direction='in'/>"
		"      <arg name='property' type='s' direction='in'/>"
		"      <arg name='value' type='v' direction='out'/>"
		"    </method>"
		"    <method name='GetAll'>"
		"      <arg name='interface' type='s' direction='in'/>"
		"      <arg name='properties' type='a{sv}' direction='out'/>"
		"    </method>"
		"    <method name='Set'>"
		"      <arg name='interface' type='s' direction='in'/>"
		"      <arg name='property' type='s' direction='in'/>"
		"      <arg name='value' type='v' direction='in'/>"
		"    </method>"
		"  </interface>"
		"  <interface name='org.freedesktop.DBus.Peer'>"
		"    <method name='Ping'/>"
		"  </interface>"
		"  <interface name='org.bluez.LEAdvertisement1'>"
		"    <method name='Release'/>"
		"    <property name='Type' type='s' access='read'/>"
		"    <property name='ServiceUUIDs' type='as' access='read'/>"
		"    <property name='LocalName' type='s' access='read'/>"
		"    <property name='Appearance' type='q' access='read'/>"
		"    <property name='Discoverable' type='b' access='read'/>"
		"  </interface>"
		"</node>";
	(void)user_data;

	if (dbus_message_is_method_call(msg, DBUS_PROPERTIES_IFACE, "GetAll"))
		return handle_get_all(conn, msg);

	if (dbus_message_is_method_call(msg, DBUS_OBJECT_MANAGER_IFACE,
					"GetManagedObjects"))
		return handle_get_managed_objects(conn, msg);

	if (dbus_message_is_method_call(msg, DBUS_PROPERTIES_IFACE, "Get"))
		return handle_get(conn, msg);

	if (dbus_message_is_method_call(msg, DBUS_PROPERTIES_IFACE, "Set"))
		return send_error(conn, msg, DBUS_ERROR_PROPERTY_READ_ONLY,
				 "properties are read-only");

	if (dbus_message_is_method_call(msg, DBUS_INTROSPECTABLE_IFACE,
					"Introspect")) {
		DBusMessage *reply = dbus_message_new_method_return(msg);
		const char *xml = introspection_xml;

		if (!reply)
			return DBUS_HANDLER_RESULT_NEED_MEMORY;

		if (!dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml,
					      DBUS_TYPE_INVALID)) {
			dbus_message_unref(reply);
			return DBUS_HANDLER_RESULT_NEED_MEMORY;
		}

		dbus_connection_send(conn, reply, NULL);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_method_call(msg, DBUS_PEER_IFACE, "Ping")) {
		DBusMessage *reply = dbus_message_new_method_return(msg);

		if (!reply)
			return DBUS_HANDLER_RESULT_NEED_MEMORY;

		dbus_connection_send(conn, reply, NULL);
		dbus_message_unref(reply);
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_method_call(msg, BLUEZ_ADV_IFACE, "Release")) {
		DBusMessage *reply = dbus_message_new_method_return(msg);

		if (!reply)
			return DBUS_HANDLER_RESULT_NEED_MEMORY;

		fprintf(stderr, "bluez_adv_helper: Release received\n");
		dbus_connection_send(conn, reply, NULL);
		dbus_message_unref(reply);
		s_running = 0;
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int register_advertisement(void)
{
	DBusError error;
	DBusMessage *msg;
	DBusMessage *reply;
	DBusPendingCall *pending = NULL;
	DBusMessageIter iter;
	DBusMessageIter dict;
	const char *path = ADV_OBJECT_PATH;
	int wait_ms = 5000;

	dbus_error_init(&error);

	msg = dbus_message_new_method_call(BLUEZ_BUS_NAME, BLUEZ_ADAPTER_PATH,
					   BLUEZ_ADV_MGR_IFACE,
					   "RegisterAdvertisement");
	if (!msg) {
		fprintf(stderr,
			"bluez_adv_helper: failed to allocate RegisterAdvertisement call\n");
		return -1;
	}

	dbus_message_iter_init_append(msg, &iter);
	if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_OBJECT_PATH,
					    &path) ||
	    !dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}",
					      &dict) ||
	    !dbus_message_iter_close_container(&iter, &dict)) {
		fprintf(stderr,
			"bluez_adv_helper: failed to encode RegisterAdvertisement args\n");
		dbus_message_unref(msg);
		return -1;
	}

	if (!dbus_connection_send_with_reply(s_connection, msg, &pending,
					     wait_ms)) {
		fprintf(stderr,
			"bluez_adv_helper: failed to send RegisterAdvertisement\n");
		dbus_message_unref(msg);
		return -1;
	}
	dbus_message_unref(msg);

	while (wait_ms > 0 && !dbus_pending_call_get_completed(pending)) {
		dbus_connection_read_write_dispatch(s_connection, 100);
		wait_ms -= 100;
	}

	if (!dbus_pending_call_get_completed(pending)) {
		fprintf(stderr,
			"bluez_adv_helper: RegisterAdvertisement timed out while waiting for BlueZ reply\n");
		dbus_pending_call_unref(pending);
		return -1;
	}

	reply = dbus_pending_call_steal_reply(pending);
	dbus_pending_call_unref(pending);
	if (!reply) {
		fprintf(stderr,
			"bluez_adv_helper: RegisterAdvertisement returned no reply object\n");
		return -1;
	}

	if (dbus_set_error_from_message(&error, reply)) {
		fprintf(stderr,
			"bluez_adv_helper: RegisterAdvertisement failed: %s: %s\n",
			error.name ? error.name : "(unknown)",
			error.message ? error.message : "(no message)");
		dbus_message_unref(reply);
		dbus_error_free(&error);
		return -1;
	}

	dbus_message_unref(reply);
	s_registered = true;
	fprintf(stderr,
		"bluez_adv_helper: RegisterAdvertisement succeeded for name=%s\n",
		s_local_name);
	return 0;
}

static void unregister_advertisement(void)
{
	DBusError error;
	DBusMessage *msg;
	DBusMessage *reply;
	const char *path = ADV_OBJECT_PATH;

	if (!s_connection || !s_registered)
		return;

	dbus_error_init(&error);
	msg = dbus_message_new_method_call(BLUEZ_BUS_NAME, BLUEZ_ADAPTER_PATH,
					   BLUEZ_ADV_MGR_IFACE,
					   "UnregisterAdvertisement");
	if (!msg)
		return;

	if (!dbus_message_append_args(msg, DBUS_TYPE_OBJECT_PATH, &path,
				      DBUS_TYPE_INVALID)) {
		dbus_message_unref(msg);
		return;
	}

	reply = dbus_connection_send_with_reply_and_block(s_connection, msg, 3000,
							  &error);
	dbus_message_unref(msg);
	if (!reply) {
		fprintf(stderr,
			"bluez_adv_helper: UnregisterAdvertisement failed: %s: %s\n",
			error.name ? error.name : "(unknown)",
			error.message ? error.message : "(no message)");
		dbus_error_free(&error);
		return;
	}

	dbus_message_unref(reply);
	s_registered = false;
	fprintf(stderr, "bluez_adv_helper: advertisement unregistered\n");
}

static int connect_system_bus(void)
{
	DBusError error;

	dbus_error_init(&error);
	s_connection = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
	if (!s_connection) {
		fprintf(stderr, "bluez_adv_helper: dbus_bus_get failed: %s: %s\n",
			error.name ? error.name : "(unknown)",
			error.message ? error.message : "(no message)");
		dbus_error_free(&error);
		return -1;
	}

	dbus_connection_set_exit_on_disconnect(s_connection, FALSE);
	return 0;
}

int main(int argc, char **argv)
{
	static const DBusObjectPathVTable adv_vtable = {
		.unregister_function = NULL,
		.message_function = advertisement_message_handler,
		.dbus_internal_pad1 = NULL,
		.dbus_internal_pad2 = NULL,
		.dbus_internal_pad3 = NULL,
		.dbus_internal_pad4 = NULL
	};

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	if (argc > 1 && argv[1][0] != '\0') {
		snprintf(s_local_name, sizeof(s_local_name), "%s", argv[1]);
	} else {
		snprintf(s_local_name, sizeof(s_local_name), "%s", "SmartOne");
	}

	fprintf(stderr, "bluez_adv_helper: starting name=%s\n", s_local_name);
	install_signal_handlers();

	if (connect_system_bus() != 0)
		return 1;

	if (!dbus_connection_register_object_path(s_connection, ADV_OBJECT_PATH,
						  &adv_vtable, NULL)) {
		fprintf(stderr,
			"bluez_adv_helper: failed to register object path %s\n",
			ADV_OBJECT_PATH);
		dbus_connection_unref(s_connection);
		return 1;
	}

	if (register_advertisement() != 0) {
		dbus_connection_unregister_object_path(s_connection,
						       ADV_OBJECT_PATH);
		dbus_connection_unref(s_connection);
		return 2;
	}

	while (s_running && dbus_connection_get_is_connected(s_connection))
		dbus_connection_read_write_dispatch(s_connection, 1000);

	fprintf(stderr, "bluez_adv_helper: stopping\n");
	unregister_advertisement();
	dbus_connection_unregister_object_path(s_connection, ADV_OBJECT_PATH);
	dbus_connection_unref(s_connection);
	s_connection = NULL;
	return 0;
}
