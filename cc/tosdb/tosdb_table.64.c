/**
 * @file tosdb_table.64.c
 * @brief tosdb table interface implementation
 *
 * This work is licensed under TURNSTONE OS Public License.
 * Please read and understand latest version of Licence.
 */

#include <tosdb/tosdb.h>
#include <tosdb/tosdb_internal.h>
#include <video.h>
#include <strings.h>

boolean_t tosdb_table_load_indexes(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "table or db is null");

        return false;
    }

    tbl->indexes = map_string();

    if(!tbl->indexes) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create index map for table %s", tbl->name);

        return false;
    }

    uint64_t idx_list_loc = tbl->index_list_location;
    uint64_t idx_list_size = tbl->index_list_size;

    while(idx_list_loc != 0) {
        tosdb_block_index_list_t* idx_list = (tosdb_block_index_list_t*)tosdb_block_read(tbl->db->tdb, idx_list_loc, idx_list_size);

        if(!idx_list) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read table list for table %s", tbl->name);

            return false;
        }

        char_t name_buf[TOSDB_NAME_MAX_LEN + 1] = {0};

        for(uint64_t i = 0; i < idx_list->index_count; i++) {
            memory_memclean(name_buf, TOSDB_NAME_MAX_LEN + 1);
            memory_memcopy(idx_list->indexes[i].name, name_buf, TOSDB_NAME_MAX_LEN);

            if(map_exists(tbl->indexes, name_buf)) {
                continue;
            }

            tosdb_index_t* idx = memory_malloc(sizeof(tosdb_index_t));

            if(!tbl) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate idx for table %s", tbl->name);
                memory_free(idx_list);

                return false;
            }

            idx->id = idx_list->indexes[i].id;
            tbl->name = strdup(name_buf);
            idx->is_deleted = idx_list->indexes[i].deleted;
            idx->type = idx_list->indexes[i].type;
            idx->column_id = idx_list->indexes[i].column_id;

            map_insert(tbl->indexes, idx->name, idx);
        }


        if(idx_list->header.previous_block_invalid) {
            memory_free(idx_list);

            break;
        }

        idx_list_loc = idx_list->header.previous_block_location;
        idx_list_size = idx_list->header.previous_block_size;

        memory_free(idx_list);
    }

    return true;
}

boolean_t tosdb_table_load_columns(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "table or db is null");

        return false;
    }

    tbl->columns = map_string();

    if(!tbl->columns) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create column map for table %s", tbl->name);

        return false;
    }

    uint64_t col_list_loc = tbl->column_list_location;
    uint64_t col_list_size = tbl->column_list_size;

    while(col_list_loc != 0) {
        tosdb_block_column_list_t* col_list = (tosdb_block_column_list_t*)tosdb_block_read(tbl->db->tdb, col_list_loc, col_list_size);

        if(!col_list) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot read table list for table %s", tbl->name);

            return false;
        }

        char_t name_buf[TOSDB_NAME_MAX_LEN + 1] = {0};

        for(uint64_t i = 0; i < col_list->column_count; i++) {
            memory_memclean(name_buf, TOSDB_NAME_MAX_LEN + 1);
            memory_memcopy(col_list->columns[i].name, name_buf, TOSDB_NAME_MAX_LEN);

            if(map_exists(tbl->columns, name_buf)) {
                continue;
            }

            tosdb_column_t* col = memory_malloc(sizeof(tosdb_column_t));

            if(!tbl) {
                PRINTLOG(TOSDB, LOG_ERROR, "cannot allocate col for table %s", tbl->name);
                memory_free(col_list);

                return false;
            }

            col->id = col_list->columns[i].id;
            tbl->name = strdup(name_buf);
            col->is_deleted = col_list->columns[i].deleted;
            col->type = col_list->columns[i].type;

            map_insert(tbl->columns, col->name, col);
        }


        if(col_list->header.previous_block_invalid) {
            memory_free(col_list);

            break;
        }

        col_list_loc = col_list->header.previous_block_location;
        col_list_size = col_list->header.previous_block_size;

        memory_free(col_list);
    }

    return true;
}

tosdb_table_t* tosdb_table_load_table(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "table or db is null");

        return NULL;
    }

    if(tbl->is_deleted) {
        PRINTLOG(TOSDB, LOG_WARNING, "tbl is deleted");
        return NULL;
    }

    if(tbl->is_open) {
        return tbl;
    }

    if(!tbl->metadata_location || !tbl->metadata_size) {
        PRINTLOG(TOSDB, LOG_ERROR, "metadata not found for %s", tbl->name);

        return NULL;
    }

    tosdb_block_table_t* tbl_block = (tosdb_block_table_t*)tosdb_block_read(tbl->db->tdb, tbl->metadata_location, tbl->metadata_size);

    if(!tbl_block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot read table %s metadata", tbl->name);

        return NULL;
    }

    tbl->column_list_location = tbl_block->column_list_location;
    tbl->column_list_size = tbl_block->column_list_size;
    tbl->column_next_id = tbl_block->column_next_id;

    if(!tosdb_table_load_columns(tbl)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot load columns of table %s", tbl->name);
    }

    tbl->index_list_location = tbl_block->index_list_location;
    tbl->index_list_size = tbl_block->index_list_size;
    tbl->index_next_id = tbl_block->index_next_id;

    if(!tosdb_table_load_indexes(tbl)) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot load indexes of table %s", tbl->name);
    }

    memory_free(tbl_block);

    tbl->is_open = true;

    PRINTLOG(TOSDB, LOG_DEBUG, "table %s loaded", tbl->name);

    return tbl;
}

tosdb_table_t* tosdb_table_create_or_open(tosdb_database_t* db, char_t* name, uint64_t max_record_count, uint64_t max_valuelog_size) {
    if(strlen(name) > TOSDB_NAME_MAX_LEN) {
        PRINTLOG(TOSDB, LOG_ERROR, "table name cannot be longer than %i", TOSDB_NAME_MAX_LEN);
        return NULL;
    }

    if(!db) {
        PRINTLOG(TOSDB, LOG_ERROR, "sdb is null");

        return NULL;
    }


    if(map_exists(db->tables, name)) {
        tosdb_table_t* tbl = (tosdb_table_t*)map_get(db->tables, name);

        if(tbl->is_deleted) {
            return NULL;
        }

        if(tbl->is_open) {
            return tbl;
        }

        tbl->max_record_count = max_record_count;
        tbl->max_valuelog_size = max_valuelog_size;

        return tosdb_table_load_table(tbl);
    }

    if(!db->table_new) {
        db->table_new = linkedlist_create_list();

        if(!db->table_new) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create new table list");

            return NULL;
        }
    }

    lock_acquire(db->lock);

    tosdb_table_t* tbl = memory_malloc(sizeof(tosdb_table_t));

    if(!tbl) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create table struct");

        lock_release(db->lock);

        return NULL;
    }


    tbl->id = db->table_next_id;

    db->table_next_id++;
    db->is_dirty = true;

    tbl->db = db;
    tbl->name = strdup(name);

    tbl->is_open = true;
    tbl->is_dirty = true;

    tbl->column_next_id = 1;
    tbl->columns = map_string();

    tbl->index_next_id = 1;
    tbl->indexes = map_string();

    tbl->max_record_count = max_record_count;
    tbl->max_valuelog_size = max_valuelog_size;


    map_insert(db->tables, name, tbl);

    linkedlist_list_insert(db->table_new, tbl);

    db->table_new_count++;


    lock_release(db->lock);

    PRINTLOG(TOSDB, LOG_DEBUG, "new table %s created", tbl->name);

    return tbl;
}

boolean_t tosdb_table_close(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_ERROR, "db or tosdb is null");

        return false;
    }


    if(tbl->is_open) {
        iterator_t* iter = map_create_iterator(tbl->columns);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create column iterator");

            return false;
        }

        while(iter->end_of_iterator(iter) != 0) {
            tosdb_column_t* col = (tosdb_column_t*)iter->get_item(iter);

            memory_free(col);

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        map_destroy(tbl->columns);

        iter = map_create_iterator(tbl->indexes);

        if(!iter) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot create index iterator");

            return false;
        }

        while(iter->end_of_iterator(iter) != 0) {
            //TODO: cleanup indexes

            iter = iter->next(iter);
        }

        iter->destroy(iter);

        map_destroy(tbl->indexes);

    }

    memory_free(tbl->name);
    lock_destroy(tbl->lock);
    memory_free(tbl);

    return true;
}

boolean_t tosdb_table_column_persist(tosdb_table_t* tbl) {
    uint64_t metadata_size = sizeof(tosdb_block_column_list_t) + sizeof(tosdb_block_column_list_item_t) * tbl->column_new_count;
    metadata_size += (TOSDB_PAGE_SIZE - (metadata_size % TOSDB_PAGE_SIZE));

    tosdb_block_column_list_t* block = memory_malloc(metadata_size);

    if(!block) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create column list");

        return false;
    }

    block->header.block_type = TOSDB_BLOCK_TYPE_COLUMN_LIST;
    block->header.block_size = metadata_size;
    block->header.previous_block_location = tbl->column_list_location;
    block->header.previous_block_size = tbl->column_list_size;

    block->database_id = tbl->db->id;
    block->table_id = tbl->id;

    iterator_t* iter = linkedlist_iterator_create(tbl->column_new);

    if(!iter) {
        PRINTLOG(TOSDB, LOG_ERROR, "cannot create column iterator");

        memory_free(block);

        return false;
    }

    block->column_count = tbl->column_new_count;

    uint64_t col_idx = 0;

    while(iter->end_of_iterator(iter) != 0) {
        tosdb_column_t* col = (tosdb_column_t*)iter->delete_item(iter);

        block->columns[col_idx].id = col->id;
        strcpy(col->name, block->columns[col_idx].name);
        block->columns[col_idx].deleted = col->is_deleted;
        block->columns[col_idx].type = col->type;

        iter = iter->next(iter);
    }

    iter->destroy(iter);

    uint64_t loc = tosdb_block_write(tbl->db->tdb, (tosdb_block_header_t*)block);

    if(loc == 0) {
        memory_free(block);

        return false;
    }

    tbl->column_list_location = loc;
    tbl->column_list_size = block->header.block_size;

    memory_free(block);


    tbl->column_new_count = 0;
    linkedlist_destroy(tbl->column_new);

    return true;
}

boolean_t tosdb_table_persist(tosdb_table_t* tbl) {
    if(!tbl || !tbl->db) {
        PRINTLOG(TOSDB, LOG_FATAL, "table or db is null");

        return false;
    }

    if(!tbl->is_dirty) {
        return true;
    }

    if(!tbl->is_open) {
        PRINTLOG(TOSDB, LOG_ERROR, "table is closed");

        return false;
    }

    boolean_t need_persist = false;


    if(tbl->column_new_count) {
        need_persist = true;

        if(!tosdb_table_column_persist(tbl)) {
            PRINTLOG(TOSDB, LOG_ERROR, "cannot persist columnt list for table %s", tbl->name);

            return false;
        }
    }


    if(!tbl->metadata_location) {
        need_persist = true;
    }

    if(need_persist) {
        tosdb_block_table_t* block = memory_malloc(TOSDB_PAGE_SIZE);

        if(!block) {
            return false;
        }

        block->header.block_size = TOSDB_PAGE_SIZE;
        block->header.block_type = TOSDB_BLOCK_TYPE_TABLE;
        block->header.previous_block_invalid = true;
        block->header.previous_block_location = tbl->metadata_location;
        block->header.previous_block_size = tbl->metadata_size;

        block->id = tbl->id;
        block->database_id = tbl->db->id;
        strcpy(tbl->name, block->name);
        block->column_next_id = tbl->column_next_id;
        block->index_next_id = tbl->index_next_id;
        block->column_list_location = tbl->column_list_location;
        block->column_list_size = tbl->column_list_size;
        block->index_list_location = tbl->index_list_location;
        block->index_list_size = tbl->index_list_size;

        uint64_t loc = tosdb_block_write(tbl->db->tdb, (tosdb_block_header_t*)block);

        if(loc == 0) {
            memory_free(block);

            return false;
        }

        tbl->metadata_location = loc;
        tbl->metadata_size = block->header.block_size;

        PRINTLOG(TOSDB, LOG_DEBUG, "table loc 0x%llx size 0x%llx", loc, block->header.block_size);

        memory_free(block);
    }

    return true;
}
