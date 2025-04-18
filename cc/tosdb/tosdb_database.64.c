/**
 * @file tosdb_database.64.c
 * @brief tosdb interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <logging.h>
#include <strings.h>

MODULE("turnstone.kernel.db");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
boolean_t tosdb_database_load_tables(tosdb_database_t* db) {
    if(!db || !db->tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "db or tosdb is null");

        return false;
    }

    if(!db->tables) {
        db->tables = hashmap_string(128);
    }

    if(!db->tables) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create table map");

        return false;
    }

    uint64_t tbl_list_loc = db->table_list_location;
    uint64_t tbl_list_size = db->table_list_size;

    while(tbl_list_loc != 0) {
        tosdb_block_table_list_t* tbl_list = (tosdb_block_table_list_t*)tosdb_block_read(db->tdb, tbl_list_loc, tbl_list_size);

        if(!tbl_list) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read table list");

            return false;
        }

        char_t name_buf[TOSDB_NAME_MAX_LEN + 1] = {0};

        for(uint64_t i = 0; i < tbl_list->table_count; i++) {
            memory_memclean(name_buf, TOSDB_NAME_MAX_LEN + 1);
            memory_memcopy(tbl_list->tables[i].name, name_buf, TOSDB_NAME_MAX_LEN);

            if(hashmap_exists(db->tables, name_buf)) {
                continue;
            }

            tosdb_table_t* tbl = memory_malloc(sizeof(tosdb_table_t));

            if(!tbl) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate tbl");
                memory_free(tbl_list);

                return false;
            }

            tbl->db = db;
            tbl->id = tbl_list->tables[i].id;
            tbl->name = strdup(name_buf);
            tbl->is_deleted = tbl_list->tables[i].deleted;
            tbl->metadata_location = tbl_list->tables[i].metadata_location;
            tbl->metadata_size = tbl_list->tables[i].metadata_size;

            hashmap_put(db->tables, tbl->name, tbl);

            PRINTLOG(TOSDB, LOG_DEBUG, "table %s of db %s is lazy loaded. md 0x%llx(0x%llx)", tbl->name, db->name, tbl->metadata_location, tbl->metadata_size);
        }


        if(tbl_list->header.previous_block_invalid) {
            memory_free(tbl_list);

            break;
        }

        tbl_list_loc = tbl_list->header.previous_block_location;
        tbl_list_size = tbl_list->header.previous_block_size;

        memory_free(tbl_list);
    }

    return true;
}
#pragma GCC diagnostic pop

tosdb_database_t* tosdb_database_load_database(tosdb_database_t* db) {
    if(!db || !db->tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "db or tosdb is null");

        return NULL;
    }

    if(db->is_deleted) {
        PRINTLOG(TOSDB, LOG_WARNING, "db is deleted");
        return NULL;
    }

    if(db->is_open) {
        return db;
    }

    if(!db->metadata_location || !db->metadata_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "metadata not found");

        return NULL;
    }

    tosdb_block_database_t* db_block = (tosdb_block_database_t*)tosdb_block_read(db->tdb, db->metadata_location, db->metadata_size);

    if(!db_block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read db %s metadata", db->name);

        return NULL;
    }

    db->table_next_id = db_block->table_next_id;
    db->table_list_location = db_block->table_list_location;
    db->table_list_size = db_block->table_list_size;

    PRINTLOG(TOSDB, LOG_DEBUG, "table list is at 0x%llx(0x%llx) for db %s", db->table_list_location, db->table_list_size, db->name);

    memory_free(db_block);

    if(!tosdb_database_load_tables(db)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot load tables");
    }

    db->is_open = true;

    PRINTLOG(TOSDB, LOG_DEBUG, "database %s loaded", db->name);

    return db;
}

tosdb_database_t* tosdb_database_create_or_open(tosdb_t* tdb, const char_t* name) {
    if(strlen(name) > TOSDB_NAME_MAX_LEN) {
        PRINTLOG(TOSDB, LOG_ERROR, "database name cannot be longer than %i", TOSDB_NAME_MAX_LEN);
        return NULL;
    }

    if(!tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "tosdb is null");

        return NULL;
    }


    if(hashmap_exists(tdb->databases, name)) {
        tosdb_database_t* db = (tosdb_database_t*)hashmap_get(tdb->databases, name);

        if(db->is_deleted) {
            PRINTLOG(TOSDB, LOG_ERROR, "db %s was deleted", db->name);

            return NULL;
        }

        if(db->is_open) {
            PRINTLOG(TOSDB, LOG_DEBUG, "db %s will be returned", db->name);

            return db;
        }

        PRINTLOG(TOSDB, LOG_DEBUG, "db %s will be lazy loaded", db->name);
        return tosdb_database_load_database(db);
    }

    if(!tdb->database_new) {
        tdb->database_new = hashmap_integer(128);

        if(!tdb->database_new) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create new database list");

            return NULL;
        }
    }

    lock_acquire(tdb->lock);

    tosdb_database_t* db = memory_malloc(sizeof(tosdb_database_t));

    if(!db) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create db struct");

        lock_release(tdb->lock);

        return NULL;
    }


    db->id = tdb->superblock->database_next_id;
    db->lock = lock_create();

    tdb->superblock->database_next_id++;
    tdb->is_dirty = true;

    db->tdb = tdb;
    db->name = strdup(name);

    db->is_open = true;
    db->is_dirty = true;

    db->table_next_id = 1;
    db->tables = hashmap_string(128);

    db->sequences = hashmap_string(128);

    hashmap_put(tdb->databases, name, db);

    hashmap_put(tdb->database_new, (void*)db->id, db);

    lock_release(tdb->lock);

    PRINTLOG(TOSDB, LOG_DEBUG, "new database %s created", db->name);

    return db;
}

boolean_t tosdb_database_close(tosdb_database_t* db) {
    if(!db || !db->tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "db or tosdb is null");

        return false;
    }

    boolean_t error = false;

    if(db->is_open) {
        PRINTLOG(TOSDB, LOG_DEBUG, "database %s will be closed", db->name);

        if(db->sequences) {
            PRINTLOG(TOSDB, LOG_TRACE, "database %s sequences will be closed", db->name);
            iterator_t* iter = hashmap_iterator_create(db->sequences);

            if(!iter) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot create sequence iterator");
                error = true;
            } else {
                while(iter->end_of_iterator(iter) != 0) {
                    tosdb_sequence_t* seq = (tosdb_sequence_t*)iter->get_item(iter);

                    if(!seq->this_record->set_int64(seq->this_record, "next_value", seq->next_value)) {
                        PRINTLOG(TOSDB, LOG_ERROR, "cannot set sequence %lli next value", seq->id);
                        error = true;
                    }

                    if(!seq->this_record->upsert_record(seq->this_record)) {
                        PRINTLOG(TOSDB, LOG_ERROR, "cannot upsert sequence %lli next value", seq->id);
                        error = true;
                    }

                    seq->this_record->destroy(seq->this_record);


                    lock_destroy(seq->lock);

                    memory_free(seq);

                    iter = iter->next(iter);
                }

                iter->destroy(iter);
            }

            hashmap_destroy(db->sequences);
        } else {
            PRINTLOG(TOSDB, LOG_TRACE, "database %s has no sequences", db->name);
        }

        iterator_t* iter = hashmap_iterator_create(db->tables);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create table iterator");

            return false;
        }

        while(iter->end_of_iterator(iter) != 0) {
            tosdb_table_t* tbl = (tosdb_table_t*)iter->get_item(iter);

            if(!tosdb_table_close(tbl)) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot close table %s", tbl->name);
                error = true;
            }

            iter = iter->next(iter);
        }


        iter->destroy(iter);
    }

    if(db->is_dirty) {
        if(!tosdb_database_persist(db)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot persist db");

            return false;
        }
    }

    db->is_open = false;
    PRINTLOG(TOSDB, LOG_DEBUG, "database %s is closed", db->name);

    return !error;
}

boolean_t tosdb_database_free(tosdb_database_t* db) {
    if(!db || !db->tdb) {
        PRINTLOG(TOSDB, LOG_ERROR, "db or tosdb is null");

        return false;
    }

    PRINTLOG(TOSDB, LOG_DEBUG, "database %s will be freed.", db->name);

    boolean_t error = false;

    if(db->tables) {
        iterator_t* iter = hashmap_iterator_create(db->tables);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create table iterator");
            error = true;
        } else {
            while(iter->end_of_iterator(iter) != 0) {
                tosdb_table_t* tbl = (tosdb_table_t*)iter->get_item(iter);

                if(!tosdb_table_free(tbl)) {
                    PRINTLOG(TOSDB, LOG_ERROR, "cannot free table %s", tbl->name);
                    error = true;
                }

                iter = iter->next(iter);
            }

            iter->destroy(iter);
        }

        hashmap_destroy(db->tables);
    }

    hashmap_destroy(db->table_new);

    memory_free(db->name);
    lock_destroy(db->lock);

    memory_free(db);
    PRINTLOG(TOSDB, LOG_DEBUG, "database freed");

    return !error;
}

boolean_t tosdb_database_persist(tosdb_database_t* db) {
    if(!db || !db->tdb) {
        PRINTLOG(TOSDB, LOG_FATAL, "db or tosdb is null");

        return false;
    }

    if(!db->is_dirty) {
        return true;
    }

    if(!db->is_open) {
        PRINTLOG(TOSDB, LOG_ERROR, "database is closed");

        return false;
    }

    boolean_t need_persist = false;


    if(db->table_new && hashmap_size(db->table_new)) {
        need_persist = true;

        boolean_t error = false;

        uint64_t metadata_size = sizeof(tosdb_block_table_list_t) + sizeof(tosdb_block_table_list_item_t) * hashmap_size(db->table_new);

        if(metadata_size % TOSDB_PAGE_SIZE) {
            metadata_size += (TOSDB_PAGE_SIZE - (metadata_size % TOSDB_PAGE_SIZE));
        }

        tosdb_block_table_list_t* block = memory_malloc(metadata_size);

        if(!block) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create database list");

            return false;
        }

        block->header.block_type = TOSDB_BLOCK_TYPE_TABLE_LIST;
        block->header.block_size = metadata_size;
        block->header.previous_block_location = db->table_list_location;
        block->header.previous_block_size = db->table_list_size;
        block->table_count = hashmap_size(db->table_new);
        block->database_id = db->id;

        iterator_t* iter = hashmap_iterator_create(db->table_new);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create database iterator");

            memory_free(block);

            return false;
        }

        uint64_t tbl_idx = 0;

        while(iter->end_of_iterator(iter) != 0) {
            tosdb_table_t* tbl = (tosdb_table_t*)iter->get_item(iter);

            if(tbl->is_dirty) {
                if(!tosdb_table_persist(tbl)) {
                    error = true;

                    break;
                }
            }

            block->tables[tbl_idx].id = tbl->id;
            strcopy(tbl->name, block->tables[tbl_idx].name);
            block->tables[tbl_idx].deleted = tbl->is_deleted;

            if(!tbl->is_deleted) {
                block->tables[tbl_idx].metadata_location = tbl->metadata_location;
                block->tables[tbl_idx].metadata_size = tbl->metadata_size;
            }

            iter = iter->next(iter);

            tbl_idx++;
        }

        iter->destroy(iter);

        if(error) {
            memory_free(block);

            return true;
        }

        uint64_t loc = tosdb_block_write(db->tdb, (tosdb_block_header_t*)block);

        if(loc == 0) {
            memory_free(block);

            return false;
        }

        db->table_list_location = loc;
        db->table_list_size = block->header.block_size;

        PRINTLOG(TOSDB, LOG_DEBUG, "db %s table list loc 0x%llx(0x%llx)", db->name, db->table_list_location, db->table_list_size);

        memory_free(block);


        hashmap_destroy(db->table_new);
        db->table_new = NULL;

    }


    if(!db->metadata_location) {
        need_persist = true;
    }

    if(need_persist || db->is_dirty) {
        tosdb_block_database_t* block = memory_malloc(TOSDB_PAGE_SIZE);

        if(!block) {
            return false;
        }

        block->header.block_size = TOSDB_PAGE_SIZE;
        block->header.block_type = TOSDB_BLOCK_TYPE_DATABASE;
        block->header.previous_block_invalid = true;
        block->header.previous_block_location = db->metadata_location;
        block->header.previous_block_size = db->metadata_size;

        block->id = db->id;
        strcopy(db->name, block->name);
        block->table_next_id = db->table_next_id;
        block->table_list_location = db->table_list_location;
        block->table_list_size = db->table_list_size;

        uint64_t loc = tosdb_block_write(db->tdb, (tosdb_block_header_t*)block);

        if(loc == 0) {
            memory_free(block);

            return false;
        }

        db->metadata_location = loc;
        db->metadata_size = block->header.block_size;

        db->tdb->is_dirty = true;
        db->is_dirty = false;

        if(!db->tdb->database_new) {
            db->tdb->database_new = hashmap_integer(128);

            if(!db->tdb->database_new) {
                memory_free(block);

                return false;
            }
        }

        hashmap_put(db->tdb->database_new, (void*)db->id, db);


        PRINTLOG(TOSDB, LOG_DEBUG, "database %s is persisted at loc 0x%llx size 0x%llx", db->name, loc, block->header.block_size);

        memory_free(block);
    }

    return true;
}
