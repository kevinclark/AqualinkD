

#ifndef IAQ_TOUCH_PROGRAMMER_H_
#define IAQ_TOUCH_PROGRAMMER_H_


unsigned char pop_iaqt_cmd(unsigned char receive_type);

void set_iaq_cansend(bool cansend);
bool iaqt_queue_cmd(unsigned char cmd);
bool iaqt_is_device_page(unsigned char page);

void *set_aqualink_iaqtouch_pump_rpm( void *ptr );
void *set_aqualink_iaqtouch_vsp_assignments( void *ptr );
void *get_aqualink_iaqtouch_setpoints( void *ptr );
void *get_aqualink_iaqtouch_freezeprotect( void *ptr );
void *get_aqualink_iaqtouch_aux_labels( void *ptr );
void *set_aqualink_iaqtouch_swg_percent( void *ptr );
void *set_aqualink_iaqtouch_swg_boost( void *ptr );
void *set_aqualink_iaqtouch_spa_heater_temp( void *ptr );
void *set_aqualink_iaqtouch_pool_heater_temp( void *ptr );
void *set_aqualink_iaqtouch_chiller_temp( void *ptr );
void *set_aqualink_iaqtouch_time( void *ptr );
void *set_aqualink_iaqtouch_pump_vs_program( void *ptr );
void *set_aqualink_iaqtouch_light_colormode( void *ptr );
void *set_aqualink_iaqtouch_device_on_off( void *ptr ); // Coalesced on/off sweep for iAQ Touch device-page buttons (RS virtual buttons, PDA)

// Coalesced on/off for iAQ Touch device-page buttons (virtual buttons on RS panels).
// iaqt_onoff_request records the latest intent and returns true when the caller must start the sweep thread.
struct aqualinkkey;
bool iaqt_onoff_request(struct aqualinkkey *button, bool isON);
bool iaqt_onoff_pending(struct aqualinkkey *button);
struct aqualinkdata;
void iaqt_onoff_abandon(struct aqualinkdata *aqdata, const char *reason);
void *set_aqualink_iaqtouch_onetouch_on_off( void *ptr ); 

int ref_iaqt_control_cmd(unsigned char **cmd);
void rem_iaqt_control_cmd(unsigned char *cmd);

#endif // IAQ_TOUCH_PROGRAMMER_H_