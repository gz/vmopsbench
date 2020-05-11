use super::PAGE_SIZE;
use crate::Bench;
use libc::*;
use std::sync::{Arc, Barrier};
use std::time::{Duration, Instant};

const PRIVATE_REGION_SIZE: usize = 1024 * 1024 * 8;
const PRIVATE_REGION_PAGE_NUM: usize = PRIVATE_REGION_SIZE / PAGE_SIZE;

#[derive(Clone)]
pub struct DWOM {
    path: &'static str,
    page: Vec<u8>,
}

impl Default for DWOM {
    fn default() -> DWOM {
        let page = vec![0xb; PAGE_SIZE];
        DWOM {
            // It doesn't work if trailing \0 isn't there in the filename.
            path: "/mnt/file.txt\0",
            page,
        }
    }
}

impl Bench for DWOM {
    fn init(&self, cores: Vec<u64>) {
        unsafe {
            let num_cores = cores.len();
            let _a = remove(self.path.as_ptr() as *const i8);
            let fd = open(self.path.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
            if fd == -1 {
                panic!("Unable to create a file");
            }
            for _core in cores {
                for _pge in 0..PRIVATE_REGION_PAGE_NUM {
                    if write(fd, self.page.as_ptr() as *const c_void, PAGE_SIZE)
                        != PAGE_SIZE as isize
                    {
                        panic!("DWOM: Write failed");
                    }
                }
            }
            let stat = {
                let mut info = std::mem::MaybeUninit::uninit();
                fstat(fd, info.as_mut_ptr());
                info.assume_init()
            };
            assert_eq!(PRIVATE_REGION_SIZE * num_cores, stat.st_size as usize);

            fsync(fd);
            close(fd);
        }
    }

    fn run(&self, b: Arc<Barrier>, duration: u64, core: u64) -> Vec<usize> {
        let mut secs = duration as usize;
        let mut iops = Vec::with_capacity(secs);

        unsafe {
            let fd = open(self.path.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
            if fd == -1 {
                panic!("Unable to open a file");
            }
            let page: &mut [i8; PAGE_SIZE] = &mut [0; PAGE_SIZE];
            let pos = (PRIVATE_REGION_SIZE * core as usize) as i64;

            b.wait();
            while secs > 0 {
                let mut ops = 0;
                let start = Instant::now();
                let end_experiment = start + Duration::from_secs(1);
                while Instant::now() < end_experiment {
                    for _i in 0..128 {
                        if pwrite(fd, page.as_ptr() as *mut c_void, PAGE_SIZE, pos)
                            != PAGE_SIZE as isize
                        {
                            panic!("DWOM: pwrite() failed");
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
