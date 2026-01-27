//
// Created by lyk on 24/06/24.
//

#ifndef FUZZERLOGGER_H
#define FUZZERLOGGER_H


/* FUZZERLOG: inlclude dlopen etc. */
#include <dlfcn.h>

typedef void (*fuzzerlog_reset_chances_handle)();
typedef void (*fuzzerlog_increase_chances_handle)();
typedef void (*fuzzerlog_reset_mutator_names_handle)();
typedef void (*fuzzerlog_add_mutator_name_handle)(char * mutator_name);
typedef int (*fuzzerlog_get_mutated_handle)();
typedef void (*fuzzerlog_new_seed_handle)(char * seed_name, char * kept_reason);
typedef void (*fuzzerlog_reset_current_seed_name_handle)();
typedef void (*fuzzerlog_set_current_seed_name_handle)(char * seed_name);
typedef int (*fuzzerlog_changed_seed_handle)();
typedef void (*fuzzerlog_set_splice_seed_name_handle)(char * seed_name);
typedef void (*fuzzerlog_chances_handle)();
typedef void (*fuzzerlog_previous_chances_handle)();
typedef void (*fuzzerlog_start_handle)();
typedef void (*fuzzerlog_end_handle)();
typedef void (*fuzzerlog_init_log_file_fs_handle)();
typedef void (*fuzzerlog_close_log_file_fs_handle)();
typedef void (*fuzzerlog_start_exec_target_handle)();
typedef unsigned long long (*fuzzerlog_end_exec_target_handle)();
typedef int (*fuzzerlog_get_chance_handle)();
typedef void (*fuzzerlog_info_handle)(const char* str);
typedef void (*fuzzerlog_warn_handle)(const char* str);
typedef void (*fuzzerlog_conf_handle)(const char* str);
typedef void (*fuzzerlog_reset_kept_reasons_handle)();
typedef void (*fuzzerlog_new_seed_multi_reason_handle)(char * seed_name);
typedef void (*fuzzerlog_add_kept_reason_handle)(char * kept_reason);


extern fuzzerlog_reset_chances_handle fuzzerlog_reset_chances;
extern fuzzerlog_increase_chances_handle fuzzerlog_increase_chances;
extern fuzzerlog_reset_mutator_names_handle fuzzerlog_reset_mutator_names;
extern fuzzerlog_add_mutator_name_handle fuzzerlog_add_mutator_name;
extern fuzzerlog_get_mutated_handle fuzzerlog_get_mutated;
extern fuzzerlog_new_seed_handle fuzzerlog_new_seed;
extern fuzzerlog_reset_current_seed_name_handle fuzzerlog_reset_current_seed_name;
extern fuzzerlog_set_current_seed_name_handle fuzzerlog_set_current_seed_name;
extern fuzzerlog_changed_seed_handle fuzzerlog_changed_seed;
extern fuzzerlog_set_splice_seed_name_handle fuzzerlog_set_splice_seed_name;
extern fuzzerlog_chances_handle fuzzerlog_chances;
extern fuzzerlog_previous_chances_handle fuzzerlog_previous_chances;
extern fuzzerlog_start_handle fuzzerlog_start;
extern fuzzerlog_end_handle fuzzerlog_end;
extern fuzzerlog_init_log_file_fs_handle fuzzerlog_init_log_file_fs;
extern fuzzerlog_close_log_file_fs_handle fuzzerlog_close_log_file_fs;
extern fuzzerlog_start_exec_target_handle fuzzerlog_start_exec_target;
extern fuzzerlog_end_exec_target_handle fuzzerlog_end_exec_target;
extern fuzzerlog_get_chance_handle fuzzerlog_get_chance;
extern fuzzerlog_info_handle fuzzerlog_info;
extern fuzzerlog_warn_handle fuzzerlog_warn;
extern fuzzerlog_conf_handle fuzzerlog_conf;
extern fuzzerlog_reset_kept_reasons_handle fuzzerlog_reset_kept_reasons;
extern fuzzerlog_new_seed_multi_reason_handle fuzzerlog_new_seed_multi_reason;
extern fuzzerlog_add_kept_reason_handle fuzzerlog_add_kept_reason;


#endif //FUZZERLOGGER_H
