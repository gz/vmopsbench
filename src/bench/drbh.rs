use super::PAGE_SIZE;
use crate::Bench;
use libc::*;
use std::sync::{Arc, Barrier};
use std::time::{Duration, Instant};

#[derive(Clone)]
pub struct DRBH {
    path: &'static str,
    page: Vec<u8>,
}

impl Default for DRBH {
    fn default() -> DRBH {
        let page = vec![0xb; PAGE_SIZE];
        DRBH {
            // It doesn't work if trailing \0 isn't there in the filename.
            path: "/mnt/file.txt\0",
            page,
        }
    }
}

impl Bench for DRBH {
    fn init(&self, _cores: Vec<u64>, _open_files: usize) {
        unsafe {
            let _a = remove(self.path.as_ptr() as *const i8);
            let fd = open(self.path.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
            if fd == -1 {
                panic!("Unable to create a file");
            }
            let len = self.page.len();
            if write(fd, self.page.as_ptr() as *const c_void, len) != len as isize {
                panic!("Write failed");
            };

            fsync(fd);
            close(fd);
        }
    }

    fn run(&self, b: Arc<Barrier>, duration: u64, _core: u64, _write_ratio: usize) -> Vec<usize> {
        let mut secs = duration as usize;
        let mut iops = Vec::with_capacity(secs);

        unsafe {
            let fd = open(self.path.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
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
                    for _i in 0..128 {
                        if pread(fd, page.as_ptr() as *mut c_void, PAGE_SIZE, 0)
                            != PAGE_SIZE as isize
                        {
                            panic!("DRBH: pread() failed");
                        };
                        ops += 1;
                    }
                }
                iops.push(ops);
                secs -= 1;
            }

            fsync(fd);
            close(fd);
        }

        iops.clone()
    }
}
