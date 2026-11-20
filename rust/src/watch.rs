//! WatchModel values (docs/HOST-CONTRACT.md §3 `watch`): the simulated clock T,
//! the alarm times, the stopwatch and the 12/24 h flag. No modes. Port of
//! cpp/src/WatchModel.cpp; the signals and SCXML events are sent by host.rs.
//!
//! The clock is a wall clock without a time zone: milliseconds since
//! 1970-01-01T00:00 in proleptic Gregorian UTC arithmetic.

pub const DAY_MS: i64 = 86_400_000;

/// Days since 1970-01-01 of a civil date (H. Hinnant's algorithm).
pub fn days_from_civil(y: i32, m: i32, d: i32) -> i64 {
    let y = if m <= 2 { y - 1 } else { y } as i64;
    let era = y.div_euclid(400);
    let yoe = y - era * 400;
    let m = m as i64;
    let doy = (153 * (if m > 2 { m - 3 } else { m + 9 }) + 2) / 5 + d as i64 - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146_097 + doe - 719_468
}

pub fn civil_from_days(z: i64) -> (i32, i32, i32) {
    let z = z + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = (doy - (153 * mp + 2) / 5 + 1) as i32;
    let m = if mp < 10 { mp + 3 } else { mp - 9 } as i32;
    let y = (yoe + era * 400 + if m <= 2 { 1 } else { 0 }) as i32;
    (y, m, d)
}

pub fn days_in_month(y: i32, m: i32) -> i32 {
    match m {
        2 if (y % 4 == 0 && y % 100 != 0) || y % 400 == 0 => 29,
        2 => 28,
        4 | 6 | 9 | 11 => 30,
        _ => 31,
    }
}

/// A wall-clock moment from date and time fields.
pub fn wall(y: i32, mo: i32, d: i32, h: i32, mi: i32, s: i32) -> i64 {
    days_from_civil(y, mo, d) * DAY_MS + ((h as i64 * 60 + mi as i64) * 60 + s as i64) * 1000
}

pub struct Watch {
    pub now: i64,                                    // ms, wall clock
    pub reset_time: Option<(i32, i32, i32, i32, i32, i32)>, // --time
    pub weekday_offset: i32,                         // 'update_day' sets the weekday independently
    pub is24h: bool,
    pub t1: i32,                                     // minutes after midnight
    pub t2: i32,
    pub sw_accum_ms: i64,                            // finished run segments
    pub sw_start_ms: i64,                            // sim_ms at last start
    pub sim_ms: i64,                                 // simulated ms since start (monotonic)
    pub lap: i64,
    pub sw_running: bool,
    pub speed: f64,
    pub carry: f64,                                  // fraction of a ms (running clock)
}

impl Default for Watch {
    fn default() -> Self {
        Watch {
            now: wall(1987, 6, 1, 12, 0, 0),
            reset_time: None,
            weekday_offset: 0,
            is24h: false,
            t1: 12 * 60,
            t2: 12 * 60,
            sw_accum_ms: 0,
            sw_start_ms: 0,
            sim_ms: 0,
            lap: 0,
            sw_running: false,
            speed: 1.0,
            carry: 0.0,
        }
    }
}

/// Increment one decimal digit of `value` (place 1 or 10) modulo `modulo`, no carry.
fn inc_digit(value: i32, place: i32, modulo: i32) -> i32 {
    let digit = (value / place) % 10;
    value - digit * place + ((digit + 1) % modulo) * place
}

/// What `increment` changed, so the host knows which signal / event follows.
pub enum Changed {
    Time,
    Settings,
    Alarms,
    Nothing,
}

impl Watch {
    fn days(&self) -> i64 { self.now.div_euclid(DAY_MS) }
    fn ms_of_day(&self) -> i64 { self.now.rem_euclid(DAY_MS) }
    pub fn date(&self) -> (i32, i32, i32) { civil_from_days(self.days()) }
    pub fn hour(&self) -> i32 { (self.ms_of_day() / 3_600_000) as i32 }
    pub fn minute(&self) -> i32 { (self.ms_of_day() / 60_000 % 60) as i32 }
    pub fn second(&self) -> i32 { (self.ms_of_day() / 1000 % 60) as i32 }
    pub fn year(&self) -> i32 { self.date().0 }
    pub fn month(&self) -> i32 { self.date().1 }
    pub fn day(&self) -> i32 { self.date().2 }
    /// 1 = Monday … 7 = Sunday (1970-01-01 was a Thursday).
    pub fn weekday(&self) -> i32 {
        let iso = (self.days() + 3).rem_euclid(7) as i32 + 1;
        (iso - 1 + self.weekday_offset) % 7 + 1
    }

    pub fn stopwatch_centis(&self) -> i64 {
        (self.sw_accum_ms + if self.sw_running { self.sim_ms - self.sw_start_ms } else { 0 }) / 10
    }

    fn set_fields(&mut self, (y, mo, d): (i32, i32, i32), h: i32, mi: i32, s: i32, ms: i64) {
        self.now = wall(y, mo, d, h, mi, s) + ms;
    }
    fn msec(&self) -> i64 { self.ms_of_day() % 1000 }

    /// Move T forward; returns the T.hits.* events, in order, for every whole
    /// minute crossed in (before, now], and whether the shown seconds changed.
    pub fn advance(&mut self, sim_ms: i64) -> (Vec<&'static str>, bool) {
        if sim_ms <= 0 {
            return (vec![], false);
        }
        let before = self.now;
        self.now += sim_ms;
        self.sim_ms += sim_ms;
        let mut hits = vec![];
        let mut t = before.div_euclid(60_000) * 60_000 + 60_000;
        while t <= self.now {
            let mod_ = (t.rem_euclid(DAY_MS) / 60_000) as i32;
            if mod_ == self.t1 {
                hits.push("T.hits.T1");
            }
            if mod_ == self.t2 {
                hits.push("T.hits.T2");
            }
            if mod_ % 60 == 0 {
                hits.push("T.hits.hour");
            }
            t += 60_000;
        }
        (hits, before.div_euclid(1000) != self.now.div_euclid(1000))
    }

    /// Clamped to 0–23 / 0–59 / 0–59; keeps the date.
    pub fn set_time(&mut self, h: i32, m: i32, s: i32) {
        self.set_fields(self.date(), h.clamp(0, 23), m.clamp(0, 59), s.clamp(0, 59), 0);
    }

    pub fn set_date(&mut self, y: i32, mo: i32, d: i32) {
        let mo = mo.clamp(1, 12);
        let d = d.clamp(1, days_in_month(y, mo));
        self.set_fields((y, mo, d), self.hour(), self.minute(), self.second(), self.msec());
    }

    /// Minutes modulo 1440 (−1 is 23:59).
    pub fn set_alarm(&mut self, which: i32, minutes: i32) {
        let m = minutes.rem_euclid(1440);
        if which == 2 { self.t2 = m } else { self.t1 = m }
    }

    /// T := next occurrence of t<which> − secs (today if not in the past, else tomorrow).
    pub fn jump_before_alarm(&mut self, which: i32, seconds: i32) {
        let target = if which == 2 { self.t2 } else { self.t1 };
        let mut t = self.days() * DAY_MS + target as i64 * 60_000;
        if t - seconds as i64 * 1000 < self.now {
            t += DAY_MS;
        }
        self.now = (t - seconds as i64 * 1000).div_euclid(1000) * 1000;
    }

    /// model.reset: power-on defaults (DESIGN-NOTES D-8). `current` is the host's
    /// local wall-clock time, used without --time.
    pub fn reset(&mut self, current: (i32, i32, i32, i32, i32, i32)) {
        let (y, mo, d, h, mi, s) = self.reset_time.unwrap_or(current);
        self.set_fields((y, mo, d), h, mi, s, 0);
        self.weekday_offset = 0;
        self.is24h = false;
        self.t1 = 12 * 60;
        self.t2 = 12 * 60;
        self.sw_accum_ms = 0;
        self.lap = 0;
        self.sw_running = false;
    }

    pub fn increment(&mut self, field: &str) -> Changed {
        let (mut h, mut mi, mut s) = (self.hour(), self.minute(), self.second());
        let (mut y, mut mo, mut d) = self.date();
        // A new time of day drops the milliseconds (QTime(h, m, s) in the C++ host);
        // a new date keeps them.
        let mut ms = self.msec();
        match field {
            "update_sec" => (s, ms) = (0, 0),
            "update_1min" => (mi, ms) = (inc_digit(mi, 1, 10), 0),
            "update_10min" => (mi, ms) = (inc_digit(mi, 10, 6), 0),
            "update_hr" => (h, ms) = ((h + 1) % 24, 0),
            "update_mon" => {
                mo = mo % 12 + 1;
                d = d.min(days_in_month(y, mo)); // the date clamps to the month's length
            }
            "update_date" => d = d % days_in_month(y, mo) + 1,
            "update_day" => self.weekday_offset = (self.weekday_offset + 1) % 7,
            "update_year" => {
                y = if y >= 2099 { 1980 } else { y + 1 };
                d = d.min(days_in_month(y, mo));
            }
            "update_mode" => {
                self.is24h = !self.is24h;
                return Changed::Settings;
            }
            f if f.starts_with("update1_") || f.starts_with("update2_") => {
                let a = if f.starts_with("update1_") { &mut self.t1 } else { &mut self.t2 };
                let (mut ah, mut am) = (*a / 60, *a % 60);
                if f.ends_with("_hr") {
                    ah = (ah + 1) % 24;
                } else if f.ends_with("_10min") {
                    am = inc_digit(am, 10, 6);
                } else if f.ends_with("_1min") {
                    am = inc_digit(am, 1, 10);
                }
                *a = ah * 60 + am;
                return Changed::Alarms;
            }
            _ => {
                eprintln!("WatchModel: unknown field '{field}'");
                return Changed::Nothing;
            }
        }
        self.set_fields((y, mo, d), h, mi, s, ms);
        Changed::Time
    }

    pub fn sw_start(&mut self) -> bool {
        if self.sw_running {
            return false;
        }
        self.sw_running = true;
        self.sw_start_ms = self.sim_ms;
        true
    }

    pub fn sw_stop(&mut self) -> bool {
        if !self.sw_running {
            return false;
        }
        self.sw_accum_ms += self.sim_ms - self.sw_start_ms; // keep ms: no rounding per segment
        self.sw_running = false;
        true
    }

    pub fn sw_lap(&mut self) {
        self.lap = self.stopwatch_centis();
    }

    pub fn sw_reset(&mut self) {
        self.sw_accum_ms = 0;
        self.lap = 0;
        self.sw_running = false;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn civil_round_trip() {
        for z in -800_000..800_000 {
            let (y, m, d) = civil_from_days(z);
            assert_eq!(days_from_civil(y, m, d), z);
        }
        assert_eq!(days_from_civil(1970, 1, 1), 0);
    }

    #[test]
    fn weekday_and_wrap() {
        let mut w = Watch::default();
        assert_eq!(w.weekday(), 1); // 1987-06-01 was a Monday
        w.set_time(23, 59, 30);
        let (hits, _) = w.advance(60_000);
        assert_eq!(hits, vec!["T.hits.hour"]);
        assert_eq!((w.year(), w.month(), w.day(), w.hour()), (1987, 6, 2, 0));
    }
}
