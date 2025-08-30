//
// Created by lyk on 24/06/24.
//

#ifndef FUZZERLOGGER_H
#define FUZZERLOGGER_H


/* FUZZERLOG: inlclude dlopen etc. */
#include <dlfcn.h>

typedef void (*reset_chances_handle)();
typedef void (*increase_chances_handle)();
typedef void (*reset_mutator_names_handle)();
typedef void (*add_mutator_name_handle)(char * mutator_name);
typedef int (*get_mutated_handle)();
typedef int (*changed_seed_handle)();
typedef void (*log_new_seed_handle)(char * seed_name, char * kept_reason);
typedef void (*reset_current_seed_name_handle)();
typedef void (*set_current_seed_name_handle)(char * seed_name);
typedef void (*set_splice_seed_name_handle)(char * seed_name);
typedef void (*log_chances_handle)();
typedef void (*log_previous_chances_handle)();
typedef void (*fuzzer_logger_start_handle)();
typedef void (*fuzzer_logger_end_handle)();
typedef void (*start_exec_target_handle)();
typedef unsigned long long (*end_exec_target_handle)();

extern reset_chances_handle reset_chances;
extern increase_chances_handle increase_chances;
extern reset_mutator_names_handle reset_mutator_names;
extern add_mutator_name_handle add_mutator_name;
extern get_mutated_handle get_mutated;
extern changed_seed_handle changed_seed;
extern log_new_seed_handle log_new_seed;
extern reset_current_seed_name_handle reset_current_seed_name;
extern set_current_seed_name_handle set_current_seed_name;
extern set_splice_seed_name_handle set_splice_seed_name;
extern log_chances_handle log_chances;
extern log_previous_chances_handle log_previous_chances;
extern fuzzer_logger_start_handle fuzzer_logger_start;
extern fuzzer_logger_end_handle fuzzer_logger_end;
extern start_exec_target_handle start_exec_target;
extern end_exec_target_handle end_exec_target;

#endif //FUZZERLOGGER_H
