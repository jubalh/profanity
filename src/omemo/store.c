#include <glib.h>
#include <signal/signal_protocol.h>

#include "config.h"
#include "omemo/omemo.h"
#include "omemo/store.h"

GHashTable *
session_store_new(void)
{
    return g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);
}

GHashTable *
pre_key_store_new(void)
{
    return g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)signal_buffer_free);
}

GHashTable *
signed_pre_key_store_new(void)
{
    return g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)signal_buffer_free);
}

void
identity_key_store_new(identity_key_store_t *identity_key_store)
{
    identity_key_store->trusted = g_hash_table_new_full(g_str_hash, g_str_equal, free, (GDestroyNotify)signal_buffer_free);
    identity_key_store->private = NULL;
    identity_key_store->public = NULL;
}

#ifdef HAVE_LIBSIGNAL_LT_2_3_2
int
load_session(signal_buffer **record, const signal_protocol_address *address,
    void *user_data)
#else
int
load_session(signal_buffer **record, signal_buffer **user_record,
    const signal_protocol_address *address, void *user_data)
#endif
{
    GHashTable *session_store = (GHashTable *)user_data;
    GHashTable *device_store = NULL;

    device_store = g_hash_table_lookup(session_store, address->name);
    if (!device_store) {
        *record = NULL;
        return 0;
    }

    signal_buffer *original = g_hash_table_lookup(device_store, GINT_TO_POINTER(address->device_id));
    if (!original) {
        *record = NULL;
        return 0;
    }
    *record = signal_buffer_copy(original);
    return 1;
}

int
get_sub_device_sessions(signal_int_list **sessions, const char *name,
    size_t name_len, void *user_data)
{
    GHashTable *session_store = (GHashTable *)user_data;
    GHashTable *device_store = NULL;
    GHashTableIter iter;
    gpointer key, value;

    device_store = g_hash_table_lookup(session_store, name);
    if (!device_store) {
        return SG_SUCCESS;
    }

    *sessions = signal_int_list_alloc();
    g_hash_table_iter_init(&iter, device_store);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        signal_int_list_push_back(*sessions, GPOINTER_TO_INT(key));
    }


    return SG_SUCCESS;
}

#ifdef HAVE_LIBSIGNAL_LT_2_3_2
int
store_session(const signal_protocol_address *address, uint8_t *record,
    size_t record_len, void *user_data)
#else
int
store_session(const signal_protocol_address *address,
    uint8_t *record, size_t record_len,
    uint8_t *user_record, size_t user_record_len,
    void *user_data)
#endif
{
    GHashTable *session_store = (GHashTable *)user_data;
    GHashTable *device_store = NULL;

    device_store = g_hash_table_lookup(session_store, (void *)address->name);
    if (!device_store) {
        device_store = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)signal_buffer_free);
        g_hash_table_insert(session_store, strdup(address->name), device_store);
    }

    signal_buffer *buffer = signal_buffer_create(record, record_len);
    g_hash_table_insert(device_store, GINT_TO_POINTER(address->device_id), buffer);


    char *record_b64 = g_base64_encode(record, record_len);
    char *device_id = g_strdup_printf("%d", address->device_id);
    g_key_file_set_string(omemo_sessions_keyfile(), address->name, device_id, record_b64);
    free(device_id);
    g_free(record_b64);

    omemo_sessions_keyfile_save();

    return SG_SUCCESS;
}

int
contains_session(const signal_protocol_address *address, void *user_data)
{
    GHashTable *session_store = (GHashTable *)user_data;
    GHashTable *device_store = NULL;

    device_store = g_hash_table_lookup(session_store, address->name);
    if (!device_store) {
        return 0;
    }

    if (!g_hash_table_lookup(device_store, GINT_TO_POINTER(address->device_id))) {
        return 0;
    }

    return 1;
}

int
delete_session(const signal_protocol_address *address, void *user_data)
{
    GHashTable *session_store = (GHashTable *)user_data;
    GHashTable *device_store = NULL;

    device_store = g_hash_table_lookup(session_store, address->name);
    if (!device_store) {
        return SG_SUCCESS;
    }

    return g_hash_table_remove(device_store, GINT_TO_POINTER(address->device_id));
}

int
delete_all_sessions(const char *name, size_t name_len, void *user_data)
{
    GHashTable *session_store = (GHashTable *)user_data;
    GHashTable *device_store = NULL;

    device_store = g_hash_table_lookup(session_store, name);
    if (!device_store) {
        return SG_SUCCESS;
    }

    guint len = g_hash_table_size(device_store);
    g_hash_table_remove_all(device_store);
    return len;
}

int
load_pre_key(signal_buffer **record, uint32_t pre_key_id, void *user_data)
{
    signal_buffer *original;
    GHashTable *pre_key_store = (GHashTable *)user_data;

    original = g_hash_table_lookup(pre_key_store, GINT_TO_POINTER(pre_key_id));
    if (original == NULL) {
        return SG_ERR_INVALID_KEY_ID;
    }

    *record = signal_buffer_copy(original);
    return SG_SUCCESS;
}

int
store_pre_key(uint32_t pre_key_id, uint8_t *record, size_t record_len,
    void *user_data)
{
    GHashTable *pre_key_store = (GHashTable *)user_data;

    signal_buffer *buffer = signal_buffer_create(record, record_len);
    g_hash_table_insert(pre_key_store, GINT_TO_POINTER(pre_key_id), buffer);

    /* Long term storage */
    char *pre_key_id_str = g_strdup_printf("%d", pre_key_id);
    char *record_b64 = g_base64_encode(record, record_len);
    g_key_file_set_string(omemo_identity_keyfile(), OMEMO_STORE_GROUP_PREKEYS, pre_key_id_str, record_b64);
    g_free(pre_key_id_str);
    g_free(record_b64);

    omemo_identity_keyfile_save();

    return SG_SUCCESS;
}

int
contains_pre_key(uint32_t pre_key_id, void *user_data)
{
    GHashTable *pre_key_store = (GHashTable *)user_data;

    return g_hash_table_lookup(pre_key_store, GINT_TO_POINTER(pre_key_id)) != NULL;
}

int
remove_pre_key(uint32_t pre_key_id, void *user_data)
{
    GHashTable *pre_key_store = (GHashTable *)user_data;

    int ret = g_hash_table_remove(pre_key_store, GINT_TO_POINTER(pre_key_id));

    /* Long term storage */
    char *pre_key_id_str = g_strdup_printf("%d", pre_key_id);
    g_key_file_remove_key(omemo_identity_keyfile(), OMEMO_STORE_GROUP_PREKEYS, pre_key_id_str, NULL);
    g_free(pre_key_id_str);

    omemo_identity_keyfile_save();

    if (ret > 0) {
        return SG_SUCCESS;
    } else {
        return SG_ERR_INVALID_KEY_ID;
    }
}

int
load_signed_pre_key(signal_buffer **record, uint32_t signed_pre_key_id,
    void *user_data)
{
    signal_buffer *original;
    GHashTable *signed_pre_key_store = (GHashTable *)user_data;

    original = g_hash_table_lookup(signed_pre_key_store, GINT_TO_POINTER(signed_pre_key_id));
    if (!original) {
        return SG_ERR_INVALID_KEY_ID;
    }

    *record = signal_buffer_copy(original);
    return SG_SUCCESS;
}

int
store_signed_pre_key(uint32_t signed_pre_key_id, uint8_t *record,
    size_t record_len, void *user_data)
{
    GHashTable *signed_pre_key_store = (GHashTable *)user_data;

    signal_buffer *buffer = signal_buffer_create(record, record_len);
    g_hash_table_insert(signed_pre_key_store, GINT_TO_POINTER(signed_pre_key_id), buffer);

    /* Long term storage */
    char *signed_pre_key_id_str = g_strdup_printf("%d", signed_pre_key_id);
    char *record_b64 = g_base64_encode(record, record_len);
    g_key_file_set_string(omemo_identity_keyfile(), OMEMO_STORE_GROUP_SIGNED_PREKEYS, signed_pre_key_id_str, record_b64);
    g_free(signed_pre_key_id_str);
    g_free(record_b64);

    omemo_identity_keyfile_save();

    return SG_SUCCESS;
}

int
contains_signed_pre_key(uint32_t signed_pre_key_id, void *user_data)
{
    GHashTable *signed_pre_key_store = (GHashTable *)user_data;

    return g_hash_table_lookup(signed_pre_key_store, GINT_TO_POINTER(signed_pre_key_id)) != NULL;
}

int
remove_signed_pre_key(uint32_t signed_pre_key_id, void *user_data)
{
    GHashTable *signed_pre_key_store = (GHashTable *)user_data;

    int ret = g_hash_table_remove(signed_pre_key_store, GINT_TO_POINTER(signed_pre_key_id));

    /* Long term storage */
    char *signed_pre_key_id_str = g_strdup_printf("%d", signed_pre_key_id);
    g_key_file_remove_key(omemo_identity_keyfile(), OMEMO_STORE_GROUP_PREKEYS, signed_pre_key_id_str, NULL);
    g_free(signed_pre_key_id_str);

    omemo_identity_keyfile_save();

    return ret;
}

int
get_identity_key_pair(signal_buffer **public_data, signal_buffer **private_data,
    void *user_data)
{
    identity_key_store_t *identity_key_store = (identity_key_store_t *)user_data;

    *public_data = signal_buffer_copy(identity_key_store->public);
    *private_data = signal_buffer_copy(identity_key_store->private);

    return SG_SUCCESS;
}

int
get_local_registration_id(void *user_data, uint32_t *registration_id)
{
    identity_key_store_t *identity_key_store = (identity_key_store_t *)user_data;

    *registration_id = identity_key_store->registration_id;

    return SG_SUCCESS;
}

int
save_identity(const signal_protocol_address *address, uint8_t *key_data,
    size_t key_len, void *user_data)
{
    identity_key_store_t *identity_key_store = (identity_key_store_t *)user_data;

    signal_buffer *buffer = signal_buffer_create(key_data, key_len);

    GHashTable *trusted = g_hash_table_lookup(identity_key_store->trusted, strdup(address->name));
    if (!trusted) {
        trusted = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)signal_buffer_free);
        g_hash_table_insert(identity_key_store->trusted, strdup(address->name), trusted);
    }
    g_hash_table_insert(trusted, GINT_TO_POINTER(address->device_id), buffer);

    /* Long term storage */
    char *key_b64 = g_base64_encode(key_data, key_len);
    char *device_id = g_strdup_printf("%d", address->device_id);
    g_key_file_set_string(omemo_trust_keyfile(), address->name, strdup(device_id), key_b64);
    g_free(device_id);
    g_free(key_b64);

    omemo_trust_keyfile_save();

    return SG_SUCCESS;
}

int
is_trusted_identity(const signal_protocol_address *address, uint8_t *key_data,
    size_t key_len, void *user_data)
{
    int ret;
    identity_key_store_t *identity_key_store = (identity_key_store_t *)user_data;

    GHashTable *trusted = g_hash_table_lookup(identity_key_store->trusted, address->name);
    if (!trusted) {
        return 0;
    }

    signal_buffer *buffer = signal_buffer_create(key_data, key_len);
    signal_buffer *original = g_hash_table_lookup(trusted, GINT_TO_POINTER(address->device_id));

    ret = original != NULL && signal_buffer_compare(buffer, original) == 0;

    signal_buffer_free(buffer);

    return ret;
}

int
store_sender_key(const signal_protocol_sender_key_name *sender_key_name,
    uint8_t *record, size_t record_len, uint8_t *user_record,
    size_t user_record_len, void *user_data)
{
    return SG_SUCCESS;
}

int
load_sender_key(signal_buffer **record, signal_buffer **user_record,
                const signal_protocol_sender_key_name *sender_key_name,
                void *user_data)
{
    return SG_SUCCESS;
}
