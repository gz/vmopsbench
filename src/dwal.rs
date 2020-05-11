use crate::Bench;
use libc::*;
use std::cell::RefCell;
use std::sync::{Arc, Barrier};
use std::time::{Duration, Instant};

#[derive(Clone)]
pub struct DWAL {
    path: &'static str,
    fds: RefCell<Vec<c_int>>,
}

unsafe impl Sync for DWAL {}

impl Default for DWAL {
    fn default() -> DWAL {
        let fd = vec![-1; 512];
        DWAL {
            path: "/mnt",
            fds: RefCell::new(fd),
        }
    }
}

impl Bench for DWAL {
    fn init(&self, cores: Vec<u64>) {
        unsafe {
            for core in cores {
                let filename = format!("{}/file{}.dat", self.path, core);

                let _a = remove(filename.as_ptr() as *const i8);
                let fd = open(filename.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
                if fd == -1 {
                    panic!("Unable to create a file");
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
                    if write(fd, page.as_ptr() as *mut c_void, page_size) != page_size as isize {
                        panic!("DWAL: write() failed");
                    }
                    ops += 1;
                }
                iops.push(ops);
                secs -= 1;
            }

            b.wait();
            close(fd);
            let filename = format!("{}/file{}.dat", self.path, core);
            remove(filename.as_ptr() as *const i8);
        }

        iops.clone()
    }
}
