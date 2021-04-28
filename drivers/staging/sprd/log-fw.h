
#ifndef LOG_FW_H_
#define LOG_FW_H_

struct forwarder;

/*
 *  start_forward - start the forward thread.
 *
 *  This function MUST be called once after the forwarding is set.
 *
 *  Return 0 on success, negative error code on error.
 */
int start_forward(struct forwarder *fw);

#endif  /* !LOG_FW_H_ */
