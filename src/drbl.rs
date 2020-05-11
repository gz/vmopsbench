use crate::Bench;
use libc::*;
use std::cell::RefCell;
use std::sync::{Arc, Barrier};
use std::time::{Duration, Instant};

#[derive(Clone)]
pub struct DRBL {
    path: &'static str,
    page: Vec<u8>,
    fds: RefCell<Vec<c_int>>,
}

unsafe impl Sync for DRBL {}

impl Default for DRBL {
    fn default() -> DRBL {
        let page = vec![0xb; 4096];
        let fd = vec![-1; 512];
        DRBL {
            // It doesn't work if trailing \0 isn't there in the filename.
            path: "/mnt",
            page,
            fds: RefCell::new(fd),
        }
    }
}

impl Bench for DRBL {
    fn init(&self, cores: Vec<u64>) {
        unsafe {
            for core in cores {
                let filename = format!("{}/file{}.txt\0", self.path, core);

                let _a = remove(filename.as_ptr() as *const i8);
                let fd = open(filename.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
                if fd == -1 {
                    panic!("Unable to create a file");
                }
                let len = self.page.len();
                if write(fd, self.page.as_ptr() as *const c_void, len) != len as isize {
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
            let page_size = 4096;
            let page: &mut [i8; 4096] = &mut [0; 4096];

            b.wait();
            while secs > 0 {
                let mut ops = 0;
                let start = Instant::now();
                let end_experiment = start + Duration::from_secs(1);
                while Instant::now() < end_experiment {
                    // pread for 128 times to reduce rdtsc overhead.
                    for _i in 0..128 {
                        if pread(fd, page.as_ptr() as *mut c_void, page_size, 0)
                            != page_size as isize
                        {
                            panic!("DRBL: pread() failed");
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
                    "DRBL: Unable to remove file, errno: {}",
                    nix::errno::errno()
                );
            }
        }

        iops.clone()
    }
}
