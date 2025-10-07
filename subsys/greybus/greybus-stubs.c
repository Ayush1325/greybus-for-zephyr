#include <stdint.h>
#include <errno.h>
#include <unipro/unipro.h>
#include <zephyr/logging/log.h>

#include "greybus-stubs.h"

LOG_MODULE_REGISTER(greybus_stubs, LOG_LEVEL_INF);

void wd_cancel(struct wdog_s *wd)
{
	(void)wd;
	LOG_DBG("");
}
void wd_delete(struct wdog_s *wd)
{
	(void)wd;
	LOG_DBG("");
}
void wd_start(struct wdog_s *wd, unsigned long delay, void (*callback)(int, uint32_t, ...),
	      int integer, uint16_t cport)
{
	(void)wd;
	LOG_DBG("");
}
void wd_static(struct wdog_s *wd)
{
	(void)wd;
	LOG_DBG("");
}

void unipro_init(void)
{
	LOG_DBG("");
}
int unipro_send(unsigned int cportid, const void *buf, size_t len)
{
	(void)cportid;
	(void)buf;
	(void)len;
	LOG_DBG("cportid: %u buf: %p len: %zu", cportid, buf, len);
	return -ENOSYS;
}
int unipro_send_async(unsigned int cportid, const void *buf, size_t len,
		      unipro_send_completion_t callback, void *priv)
{
	(void)cportid;
	(void)buf;
	(void)len;
	(void)callback;
	(void)priv;
	LOG_DBG("");
	return -ENOSYS;
}
int unipro_reset_cport(unsigned int cportid, cport_reset_completion_cb_t cb, void *priv)
{
	(void)cportid;
	(void)cb;
	(void)priv;
	LOG_DBG("cportid: %u cb: %p priv: %p", cportid, cb, priv);
	return -ENOSYS;
}

int unipro_attr_access(uint16_t attr, uint32_t *val, uint16_t selector, int peer, int write)
{
	(void)attr;
	(void)val;
	(void)selector;
	(void)peer;
	(void)write;
	LOG_DBG("attr: %u val: %p selector: %u peer: %d write: %d", attr, val, selector, peer,
		write);
	return -ENOSYS;
}

int unipro_disable_fct_tx_flow(unsigned int cport)
{
	LOG_DBG("cport: %u", cport);
	return -ENOSYS;
}
int unipro_enable_fct_tx_flow(unsigned int cport)
{
	LOG_DBG("cport: %u", cport);
	return -ENOSYS;
}

void timesync_enable()
{
}
void timesync_disable()
{
}
void timesync_authoritative()
{
}
void timesync_get_last_event()
{
}
void gb_loopback_log_exit()
{
}
void gb_loopback_log_entry()
{
}
