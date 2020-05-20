use super::{calculate_throughput, PAGE_SIZE};
use crate::Bench;
use libc::*;
use std::cell::RefCell;
use std::sync::{Arc, Barrier};
use std::time::Instant;

#[derive(Clone)]
pub struct DWAL {
    path: &'static str,
    fds: RefCell<Vec<c_int>>,
    num_appends: u64,
    page: Vec<u8>,
}

unsafe impl Sync for DWAL {}

impl Default for DWAL {
    fn default() -> DWAL {
        let fd = vec![-1; 512];
        let page = vec![0xb; PAGE_SIZE];
        DWAL {
            // It doesn't work if trailing \0 isn't there in the filename.
            path: "/mnt",
            fds: RefCell::new(fd),
            num_appends: 20_000,
            page,
        }
    }
}

impl Bench for DWAL {
    fn init(&self, cores: Vec<u64>) {
        unsafe {
            for core in cores {
                let dir_path = format!("{}/{}/\0", self.path, core);
                rmdir(dir_path.as_ptr() as *const i8);
                if mkdir(dir_path.as_ptr() as *const i8, S_IRWXU) != 0 {
                    panic!("MWRM: Unable to create directory {}", nix::errno::errno());
                }
                let filename = format!("{}/{}/file{}.dat\0", self.path, core, core);

                let _a = remove(filename.as_ptr() as *const i8);
                let fd = open(filename.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
                if fd == -1 {
                    panic!("Unable to create a file");
                }

                self.fds.borrow_mut()[core as usize] = fd;
            }
        }
    }

    fn run(&self, b: Arc<Barrier>, duration: u64, core: u64, _write_ratio: usize) -> Vec<usize> {
        let mut secs = duration as usize;
        let mut iops = Vec::with_capacity(secs);

        unsafe {
            let fd = self.fds.borrow()[core as usize];
            if fd == -1 {
                panic!("Unable to open a file");
            }

            b.wait();
            while secs > 0 {
                let mut ops = 0;
                let start = Instant::now();
                for _i in 0..self.num_appends {
                    if write(fd, self.page.as_ptr() as *mut c_void, PAGE_SIZE) != PAGE_SIZE as isize
                    {
                        panic!("DWAL: write() failed");
                    }
                    ops += 1;
                }
                let stop = Instant::now();
                let throughput = calculate_throughput(ops, stop - start);
                iops.push(throughput);
                secs -= 1;
            }

            b.wait();
            close(fd);
            let filename = format!("{}/{}/file{}.dat\0", self.path, core, core);
            if remove(filename.as_ptr() as *const i8) != 0 {
                panic!(
                    "DWAL: Unable to remove file, errno: {}",
                    nix::errno::errno()
                );
            }

            let dir_path = format!("{}/{}/\0", self.path, core);
            if rmdir(dir_path.as_ptr() as *const i8) != 0 {
                panic!(
                    "MWRM: Unable to remove file, errno: {}",
                    nix::errno::errno()
                );
            }
        }

        iops.clone()
    }
}
