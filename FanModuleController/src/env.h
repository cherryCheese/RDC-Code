/*
 * env.h
 *
 * Created: 1/19/2021 6:24:26 PM
 *  Author: E1210640
 */ 


#ifndef ENV_H_
#define ENV_H_

void env_init(void);
void env_reset(void);
int env_find(const char *var);
int env_set(const char *var, uint32_t val);
uint32_t env_get(const char *var);
void env_print_all(void);
void do_env(void);

#endif /* ENV_H_ */