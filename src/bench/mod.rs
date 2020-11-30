use std::convert::TryInto;
use std::time::Duration;

pub mod drbh;
pub mod drbl;
pub mod dwal;
pub mod dwol;
pub mod dwom;
pub mod mix;
pub mod mwrl;
pub mod mwrm;

const PAGE_SIZE: usize = 1024;

pub fn calculate_throughput(ops: u64, time: Duration) -> usize {
    let nano_duration = time.as_nanos();
    let nano_per_operation = nano_duration / ops as u128;
    (Duration::from_secs(1).as_nanos() / nano_per_operation)
        .try_into()
        .unwrap()
}
