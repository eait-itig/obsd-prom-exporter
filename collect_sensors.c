/*
 *
 * Copyright 2026 psergiu
 * Author: Sergiu Partenie with assistance from locally run LLMs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>

#include <sys/sysctl.h>
#include <sys/sensors.h>

#include "metrics.h"
#include "log.h"

/*
 * One gauge metric per sensor type, mirroring how print_sensor()
 * in sbin/sysctl/sysctl.c interprets struct sensor values.
 * Labels: device (sensordev xname, e.g. "it0"), sensor (e.g. "temp1"),
 * desc (sensor description, may be empty).
 *
 * Metrics are registered lazily on first sight of a sensor of that
 * type, so machines without a given sensor type emit nothing at all
 * for it (no bare # HELP / # TYPE lines).
 */
struct sensors_modpriv {
	struct registry *reg;
	struct metric *temp;
	struct metric *fan;
	struct metric *volt_dc;
	struct metric *volt_ac;
	struct metric *ohms;
	struct metric *watts;
	struct metric *amps;
	struct metric *watthour;
	struct metric *amphour;
	struct metric *indicator;
	struct metric *integer;
	struct metric *percent;
	struct metric *lux;
	struct metric *drive;
	struct metric *timedelta;
	struct metric *humidity;
	struct metric *freq;
	struct metric *angle;
	struct metric *distance;
	struct metric *pressure;
	struct metric *accel;
	struct metric *velocity;
	struct metric *energy;
};

struct metric_ops sensors_metric_ops = {
	.mo_collect = NULL,
	.mo_free = NULL
};

static void
sensors_register(struct registry *r, void **modpriv)
{
	struct sensors_modpriv *priv;

	priv = calloc(1, sizeof (struct sensors_modpriv));
	*modpriv = priv;
	if (priv == NULL)
		return;
	priv->reg = r;
}

static struct metric *
ensure_temp(struct sensors_modpriv *priv)
{
	if (priv->temp == NULL) {
		priv->temp = metric_new(priv->reg, "sensor_temp_celsius",
		    "Temperature sensor reading in degrees Celsius (uK -> degC)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->temp);
}

static struct metric *
ensure_fan(struct sensors_modpriv *priv)
{
	if (priv->fan == NULL) {
		priv->fan = metric_new(priv->reg, "sensor_fan_rpm",
		    "Fan speed sensor reading in RPM",
		    METRIC_GAUGE, METRIC_VAL_UINT64, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->fan);
}

static struct metric *
ensure_volt_dc(struct sensors_modpriv *priv)
{
	if (priv->volt_dc == NULL) {
		priv->volt_dc = metric_new(priv->reg,
		    "sensor_voltage_dc_volts",
		    "DC voltage sensor reading in Volts (uV DC -> V)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->volt_dc);
}

static struct metric *
ensure_volt_ac(struct sensors_modpriv *priv)
{
	if (priv->volt_ac == NULL) {
		priv->volt_ac = metric_new(priv->reg,
		    "sensor_voltage_ac_volts",
		    "AC voltage sensor reading in Volts (uV AC -> V)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->volt_ac);
}

static struct metric *
ensure_ohms(struct sensors_modpriv *priv)
{
	if (priv->ohms == NULL) {
		priv->ohms = metric_new(priv->reg, "sensor_resistance_ohms",
		    "Resistance sensor reading in Ohms",
		    METRIC_GAUGE, METRIC_VAL_INT64, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->ohms);
}

static struct metric *
ensure_watts(struct sensors_modpriv *priv)
{
	if (priv->watts == NULL) {
		priv->watts = metric_new(priv->reg, "sensor_power_watts",
		    "Power sensor reading in Watts (uW -> W)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->watts);
}

static struct metric *
ensure_amps(struct sensors_modpriv *priv)
{
	if (priv->amps == NULL) {
		priv->amps = metric_new(priv->reg,
		    "sensor_current_amperes",
		    "Current sensor reading in Amperes (uA -> A)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->amps);
}

static struct metric *
ensure_watthour(struct sensors_modpriv *priv)
{
	if (priv->watthour == NULL) {
		priv->watthour = metric_new(priv->reg,
		    "sensor_energy_watthours",
		    "Energy capacity sensor reading in Watt-hours (uWh -> Wh)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->watthour);
}

static struct metric *
ensure_amphour(struct sensors_modpriv *priv)
{
	if (priv->amphour == NULL) {
		priv->amphour = metric_new(priv->reg,
		    "sensor_charge_amphours",
		    "Charge capacity sensor reading in Ampere-hours (uAh -> Ah)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->amphour);
}

static struct metric *
ensure_indicator(struct sensors_modpriv *priv)
{
	if (priv->indicator == NULL) {
		priv->indicator = metric_new(priv->reg, "sensor_indicator_on",
		    "Boolean indicator sensor (1 = On, 0 = Off)",
		    METRIC_GAUGE, METRIC_VAL_UINT64, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->indicator);
}

static struct metric *
ensure_integer(struct sensors_modpriv *priv)
{
	if (priv->integer == NULL) {
		priv->integer = metric_new(priv->reg, "sensor_raw_value",
		    "Generic integer sensor reading",
		    METRIC_GAUGE, METRIC_VAL_INT64, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->integer);
}

static struct metric *
ensure_percent(struct sensors_modpriv *priv)
{
	if (priv->percent == NULL) {
		priv->percent = metric_new(priv->reg, "sensor_percent",
		    "Percentage sensor reading in percent (m% -> %)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->percent);
}

static struct metric *
ensure_lux(struct sensors_modpriv *priv)
{
	if (priv->lux == NULL) {
		priv->lux = metric_new(priv->reg, "sensor_illuminance_lux",
		    "Illuminance sensor reading in lux (ulx -> lx)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->lux);
}

static struct metric *
ensure_drive(struct sensors_modpriv *priv)
{
	if (priv->drive == NULL) {
		priv->drive = metric_new(priv->reg, "sensor_drive_state",
		    "Drive sensor state (1=empty, 2=ready, 3=powering up, 4=online, 5=idle, 6=active, 7=rebuilding, 8=powering down, 9=failed, 10=degraded)",
		    METRIC_GAUGE, METRIC_VAL_INT64, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->drive);
}

static struct metric *
ensure_timedelta(struct sensors_modpriv *priv)
{
	if (priv->timedelta == NULL) {
		priv->timedelta = metric_new(priv->reg,
		    "sensor_timedelta_seconds",
		    "System time error sensor reading in seconds (nSec -> secs)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->timedelta);
}

static struct metric *
ensure_humidity(struct sensors_modpriv *priv)
{
	if (priv->humidity == NULL) {
		priv->humidity = metric_new(priv->reg,
		    "sensor_humidity_percent",
		    "Humidity sensor reading in percent relative humidity (m%RH -> %)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->humidity);
}

static struct metric *
ensure_freq(struct sensors_modpriv *priv)
{
	if (priv->freq == NULL) {
		priv->freq = metric_new(priv->reg,
		    "sensor_frequency_hertz",
		    "Frequency sensor reading in Hertz (uHz -> Hz)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->freq);
}

static struct metric *
ensure_angle(struct sensors_modpriv *priv)
{
	if (priv->angle == NULL) {
		priv->angle = metric_new(priv->reg,
		    "sensor_angle_degrees",
		    "Angle sensor reading in degrees (uDegrees -> degrees)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->angle);
}

static struct metric *
ensure_distance(struct sensors_modpriv *priv)
{
	if (priv->distance == NULL) {
		priv->distance = metric_new(priv->reg,
		    "sensor_distance_meters",
		    "Distance sensor reading in meters (uMeter -> m)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->distance);
}

static struct metric *
ensure_pressure(struct sensors_modpriv *priv)
{
	if (priv->pressure == NULL) {
		priv->pressure = metric_new(priv->reg,
		    "sensor_pressure_pascals",
		    "Pressure sensor reading in Pascals (mPa -> Pa)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->pressure);
}

static struct metric *
ensure_accel(struct sensors_modpriv *priv)
{
	if (priv->accel == NULL) {
		priv->accel = metric_new(priv->reg,
		    "sensor_acceleration_meters_per_sec_squared",
		    "Acceleration sensor reading in m/s^2 (u m/s^2 -> m/s^2)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->accel);
}

static struct metric *
ensure_velocity(struct sensors_modpriv *priv)
{
	if (priv->velocity == NULL) {
		priv->velocity = metric_new(priv->reg,
		    "sensor_velocity_meters_per_second",
		    "Velocity sensor reading in m/s (u m/s -> m/s)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->velocity);
}

static struct metric *
ensure_energy(struct sensors_modpriv *priv)
{
	if (priv->energy == NULL) {
		priv->energy = metric_new(priv->reg,
		    "sensor_energy_joules",
		    "Energy sensor reading in Joules (uJ -> J)",
		    METRIC_GAUGE, METRIC_VAL_DOUBLE, NULL,
		    &sensors_metric_ops,
		    metric_label_new("device", METRIC_VAL_STRING),
		    metric_label_new("sensor", METRIC_VAL_STRING),
		    metric_label_new("desc", METRIC_VAL_STRING),
		    NULL);
	}
	return (priv->energy);
}

static int
sensors_collect(void *modpriv)
{
	struct sensors_modpriv *priv = modpriv;
	struct sensordev snsrdev;
	struct sensor snsr;
	struct metric *m;
	size_t sdlen, slen;
	int mib[5];
	int dev, type, numt;
	char sensorname[64];
	char desc[33];

	if (priv == NULL || priv->reg == NULL)
		return (0);

	/*
	 * Walk sensor devices: mib { CTL_HW, HW_SENSORS, dev } returns
	 * a struct sensordev. Device numbers may be sparse (ENXIO) and
	 * the list ends with ENOENT (same iteration as sbin/sysctl
	 * sysctl_sensors()).
	 */
	for (dev = 0; ; dev++) {
		mib[0] = CTL_HW;
		mib[1] = HW_SENSORS;
		mib[2] = dev;
		sdlen = sizeof (snsrdev);
		bzero(&snsrdev, sdlen);
		if (sysctl(mib, 3, &snsrdev, &sdlen, NULL, 0) == -1) {
			if (errno == ENXIO)
				continue;
			if (errno == ENOENT)
				break;
			tslog("failed to get sensordev %d: %s", dev,
			    strerror(errno));
			return (0);
		}
		if (sdlen == 0)
			continue;

		for (type = 0; type < SENSOR_MAX_TYPES; type++) {
			for (numt = 0; numt < snsrdev.maxnumt[type]; numt++) {
				mib[3] = type;
				mib[4] = numt;
				slen = sizeof (snsr);
				bzero(&snsr, slen);
				if (sysctl(mib, 5, &snsr, &slen, NULL, 0) == -1)
					continue;
				if (slen == 0)
					continue;
				if (snsr.flags & SENSOR_FINVALID)
					continue;
				if (snsr.flags & SENSOR_FUNKNOWN)
					continue;

				snprintf(sensorname, sizeof (sensorname),
				    "%s%d", sensor_type_s[type], numt);
				bzero(desc, sizeof (desc));
				if (snsr.desc[0] != '\0')
					snprintf(desc, sizeof (desc), "%s",
					    snsr.desc);

				/*
				 * Conversions mirror print_sensor():
				 * temp: (uK - 273150000) / 1e6 -> degC
				 * fan: raw RPM
				 * volts: uV / 1e6 -> V
				 * ohms: raw
				 * watts: uW / 1e6 -> W
				 * amps: uA / 1e6 -> A
				 * watthour: uWh / 1e6 -> Wh
				 * amphour: uAh / 1e6 -> Ah
				 * indicator: On/Off -> 1/0
				 * integer: raw
				 * percent: m% / 1e3 -> %
				 * lux: ulx / 1e6 -> lx
				 * drive: raw state enum
				 * timedelta: nSec / 1e9 -> secs
				 * humidity: m%RH / 1e3 -> %
				 * freq: uHz / 1e6 -> Hz
				 * angle: uDegrees / 1e6 -> degrees
				 * distance: uMeter / 1e6 -> m
				 * pressure: mPa / 1e3 -> Pa
				 * accel: u m/s^2 / 1e6 -> m/s^2
				 * velocity: u m/s / 1e6 -> m/s
				 * energy: uJ / 1e6 -> J
				 */
				switch (snsr.type) {
				case SENSOR_TEMP:
					m = ensure_temp(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    (snsr.value - 273150000) / 1000000.0);
					break;
				case SENSOR_FANRPM:
					m = ensure_fan(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    (uint64_t)snsr.value);
					break;
				case SENSOR_VOLTS_DC:
					m = ensure_volt_dc(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_VOLTS_AC:
					m = ensure_volt_ac(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_OHMS:
					m = ensure_ohms(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    (int64_t)snsr.value);
					break;
				case SENSOR_WATTS:
					m = ensure_watts(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_AMPS:
					m = ensure_amps(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_WATTHOUR:
					m = ensure_watthour(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_AMPHOUR:
					m = ensure_amphour(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_INDICATOR:
					m = ensure_indicator(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value ? (uint64_t)1 : (uint64_t)0);
					break;
				case SENSOR_INTEGER:
					m = ensure_integer(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    (int64_t)snsr.value);
					break;
				case SENSOR_PERCENT:
					m = ensure_percent(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000.0);
					break;
				case SENSOR_LUX:
					m = ensure_lux(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_DRIVE:
					m = ensure_drive(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    (int64_t)snsr.value);
					break;
				case SENSOR_TIMEDELTA:
					m = ensure_timedelta(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000000.0);
					break;
				case SENSOR_HUMIDITY:
					m = ensure_humidity(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000.0);
					break;
				case SENSOR_FREQ:
					m = ensure_freq(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_ANGLE:
					m = ensure_angle(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_DISTANCE:
					m = ensure_distance(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_PRESSURE:
					m = ensure_pressure(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000.0);
					break;
				case SENSOR_ACCEL:
					m = ensure_accel(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_VELOCITY:
					m = ensure_velocity(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				case SENSOR_ENERGY:
					m = ensure_energy(priv);
					if (m != NULL)
						metric_update(m,
						    snsrdev.xname, sensorname,
						    desc,
						    snsr.value / 1000000.0);
					break;
				default:
					break;
				}
			}
		}
	}

	if (priv->temp != NULL)
		metric_clear_old_values(priv->temp);
	if (priv->fan != NULL)
		metric_clear_old_values(priv->fan);
	if (priv->volt_dc != NULL)
		metric_clear_old_values(priv->volt_dc);
	if (priv->volt_ac != NULL)
		metric_clear_old_values(priv->volt_ac);
	if (priv->ohms != NULL)
		metric_clear_old_values(priv->ohms);
	if (priv->watts != NULL)
		metric_clear_old_values(priv->watts);
	if (priv->amps != NULL)
		metric_clear_old_values(priv->amps);
	if (priv->watthour != NULL)
		metric_clear_old_values(priv->watthour);
	if (priv->amphour != NULL)
		metric_clear_old_values(priv->amphour);
	if (priv->indicator != NULL)
		metric_clear_old_values(priv->indicator);
	if (priv->integer != NULL)
		metric_clear_old_values(priv->integer);
	if (priv->percent != NULL)
		metric_clear_old_values(priv->percent);
	if (priv->lux != NULL)
		metric_clear_old_values(priv->lux);
	if (priv->drive != NULL)
		metric_clear_old_values(priv->drive);
	if (priv->timedelta != NULL)
		metric_clear_old_values(priv->timedelta);
	if (priv->humidity != NULL)
		metric_clear_old_values(priv->humidity);
	if (priv->freq != NULL)
		metric_clear_old_values(priv->freq);
	if (priv->angle != NULL)
		metric_clear_old_values(priv->angle);
	if (priv->distance != NULL)
		metric_clear_old_values(priv->distance);
	if (priv->pressure != NULL)
		metric_clear_old_values(priv->pressure);
	if (priv->accel != NULL)
		metric_clear_old_values(priv->accel);
	if (priv->velocity != NULL)
		metric_clear_old_values(priv->velocity);
	if (priv->energy != NULL)
		metric_clear_old_values(priv->energy);

	return (0);
}

static void
sensors_free(void *modpriv)
{
	struct sensors_modpriv *priv = modpriv;
	free(priv);
}

struct metrics_module_ops collect_sensors_ops = {
	.mm_register = sensors_register,
	.mm_collect = sensors_collect,
	.mm_free = sensors_free
};
