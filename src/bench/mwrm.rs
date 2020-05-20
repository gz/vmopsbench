use super::calculate_throughput;
use crate::Bench;
use libc::*;
use std::cell::RefCell;
use std::sync::{Arc, Barrier};
use std::time::Instant;

#[derive(Clone)]
pub struct MWRM {
    path: &'static str,
    total_files: u64,
    total_cores: RefCell<u64>,
}

unsafe impl Sync for MWRM {}

impl Default for MWRM {
    fn default() -> MWRM {
        MWRM {
            // It doesn't work if trailing \0 isn't there in the filename.
            path: "/mnt",
            total_files: 100_000,
            total_cores: RefCell::new(0),
        }
    }
}

impl Bench for MWRM {
    fn init(&self, cores: Vec<u64>) {
        let len = cores.len();
        *self.total_cores.borrow_mut() = len as u64;
        let files_per_core = self.total_files / len as u64;
        unsafe {
            for core in cores {
                let dir_path = format!("{}/{}/\0", self.path, core);
                rmdir(dir_path.as_ptr() as *const i8);
                if mkdir(dir_path.as_ptr() as *const i8, S_IRWXU) != 0 {
                    panic!("MWRM: Unable to create directory {}", nix::errno::errno());
                }

                for i in 0..files_per_core {
                    let filename = format!("{}/{}/file-{}-{}.txt\0", self.path, core, core, i);
                    let _a = remove(filename.as_ptr() as *const i8);
                    let fd = open(filename.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
                    if fd == -1 {
                        panic!("Unable to create a file {}", nix::errno::errno());
                    }
                    fsync(fd);
                    close(fd);
                }
            }
        }
    }

    fn run(&self, b: Arc<Barrier>, duration: u64, core: u64, _write_ratio: usize) -> Vec<usize> {
        let secs = duration as usize;
        let mut iops = Vec::with_capacity(secs);
        let files_per_core = self.total_files / *self.total_cores.borrow();

        unsafe {
            let mut ops = 0;
            b.wait();

            let start = Instant::now();
            for iter in 0..files_per_core {
                let old_name = format!("{}/{}/file-{}-{}.txt\0", self.path, core, core, iter);
                let new_name = format!("{}/file-{}-{}.txt\0", self.path, core, iter);
                if rename(
                    old_name.as_ptr() as *const i8,
                    new_name.as_ptr() as *const i8,
                ) != 0
                {
                    panic!(
                        "MWRM: Unable to rename file, errno: {}",
                        nix::errno::errno()
                    );
                }
                ops += 1;
            }
            let stop = Instant::now();
            let throughput = calculate_throughput(ops, stop - start);

            // Just to avoid changing the report_bench()
            // which expects `duration` number of readings.
            for _i in 0..secs {
                iops.push(throughput);
            }

            for iter in 0..files_per_core {
                let filename = format!("{}/file-{}-{}.txt\0", self.path, core, iter);
                if remove(filename.as_ptr() as *const i8) != 0 {
                    panic!(
                        "MWRM: Unable to remove file, errno: {}",
                        nix::errno::errno()
                    );
                }
            }

            if core == 0 {
                let dir_path = format!("{}/{}/\0", self.path, core);
                if rmdir(dir_path.as_ptr() as *const i8) != 0 {
                    panic!(
                        "MWRM: Unable to remove file, errno: {}",
                        nix::errno::errno()
                    );
                }
            }
        }

        iops.clone()
    }
}
