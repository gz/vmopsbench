use super::PAGE_SIZE;
use crate::Bench;
use libc::*;
use std::cell::RefCell;
use std::sync::{Arc, Barrier};
use std::time::{Duration, Instant};

#[derive(Clone)]
pub struct DWOL {
    path: &'static str,
    page: Vec<u8>,
    fds: RefCell<Vec<c_int>>,
}

unsafe impl Sync for DWOL {}

impl Default for DWOL {
    fn default() -> DWOL {
        let page = vec![0xb; PAGE_SIZE];
        let fd = vec![-1; 512];
        DWOL {
            // It doesn't work if trailing \0 isn't there in the filename.
            path: "/mnt",
            page,
            fds: RefCell::new(fd),
        }
    }
}

impl Bench for DWOL {
    fn init(&self, cores: Vec<u64>) {
        unsafe {
            for core in cores {
                let filename = format!("{}/file{}.txt\0", self.path, core);

                let _a = remove(filename.as_ptr() as *const i8);
                let fd = open(filename.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
                if fd == -1 {
                    panic!("Unable to create a file");
                }
                if write(fd, self.page.as_ptr() as *const c_void, PAGE_SIZE) != PAGE_SIZE as isize {
                    panic!("Write failed");
                }
                self.fds.borrow_mut()[core as usize] = fd;
            }
        }
    }

    fn run(&self, b: Arc<Barrier>, duration: u64, core: u64) -> Vec<usize> {
        let mut secs = duration as usize;
        let mut iops = Vec::with_capacity(secs);

        unsafe {
            let fd = self.fds.borrow()[core as usize];
            if fd == -1 {
                panic!("Unable to open a file");
            }
            let page: &mut [i8; PAGE_SIZE] = &mut [0; PAGE_SIZE];

            b.wait();
            while secs > 0 {
                let mut ops = 0;
                let start = Instant::now();
                let end_experiment = start + Duration::from_secs(1);
                while Instant::now() < end_experiment {
                    // pread for 128 times to reduce rdtsc overhead.
                    for _i in 0..128 {
                        if pwrite(fd, page.as_ptr() as *mut c_void, PAGE_SIZE, 0)
                            != PAGE_SIZE as isize
                        {
                            panic!("DWOL: pwrite() failed");
                        }
                        ops += 1;
                    }
                }
                iops.push(ops);
                secs -= 1;
            }

            close(fd);
            let filename = format!("{}/file{}.txt\0", self.path, core);
            if remove(filename.as_ptr() as *const i8) != 0 {
                panic!(
                    "DWOL: Unable to remove file, errno: {}",
                    nix::errno::errno()
                );
            }
        }

        iops.clone()
    }
}
