use super::PAGE_SIZE;
use crate::Bench;
use libc::*;
use std::sync::{Arc, Barrier};
use std::time::{Duration, Instant};
use x86::random::rdrand16;

#[derive(Clone)]
pub struct MIX {
    path: &'static str,
    page: Vec<u8>,
    file_size: i64,
}

impl Default for MIX {
    fn default() -> MIX {
        let page = vec![0xb; PAGE_SIZE];
        MIX {
            // It doesn't work if trailing \0 isn't there in the filename.
            path: "/mnt/file.txt\0",
            page,
            file_size: 512 * 1024 * 1024,
        }
    }
}

impl Bench for MIX {
    fn init(&self, _cores: Vec<u64>) {
        unsafe {
            let _a = remove(self.path.as_ptr() as *const i8);
            let fd = open(self.path.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
            if fd == -1 {
                panic!("Unable to create a file");
            }
            let mut size = 0;
            while size <= self.file_size {
                if write(fd, self.page.as_ptr() as *const c_void, PAGE_SIZE) != PAGE_SIZE as isize {
                    panic!("MIX: Write failed");
                }
                size += PAGE_SIZE as i64;
            }

            let stat = {
                let mut info = std::mem::MaybeUninit::uninit();
                fstat(fd, info.as_mut_ptr());
                info.assume_init()
            };
            assert_eq!(self.file_size + PAGE_SIZE as i64, stat.st_size);

            fsync(fd);
            close(fd);
        }
    }

    fn run(&self, b: Arc<Barrier>, duration: u64, _core: u64, write_ratio: usize) -> Vec<usize> {
        let mut secs = duration as usize;
        let mut iops = Vec::with_capacity(secs);

        unsafe {
            let fd = open(self.path.as_ptr() as *const i8, O_CREAT | O_RDWR, S_IRWXU);
            if fd == -1 {
                panic!("Unable to open a file");
            }
            let total_pages = self.file_size / PAGE_SIZE as i64;
            let page: &mut [i8; PAGE_SIZE as usize] = &mut [0; PAGE_SIZE as usize];

            let mut random_num: u16 = 0;

            b.wait();
            while secs > 0 {
                let mut ops = 0;
                let start = Instant::now();
                let end_experiment = start + Duration::from_secs(1);
                while Instant::now() < end_experiment {
                    for _i in 0..128 {
                        rdrand16(&mut random_num);
                        let rand = random_num as i64 % total_pages;
                        let offset = rand * PAGE_SIZE as i64;

                        if random_num as usize % 100 < write_ratio {
                            if pwrite(fd, page.as_ptr() as *mut c_void, PAGE_SIZE, offset)
                                != PAGE_SIZE as isize
                            {
                                panic!("MIX: pwrite() failed {}", nix::errno::errno());
                            };
                        } else {
                            if pread(fd, page.as_ptr() as *mut c_void, PAGE_SIZE, offset)
                                != PAGE_SIZE as isize
                            {
                                panic!("MIX: pread() failed {}", nix::errno::errno());
                            };
                        }
                        ops += 1;
                    }
                }
                iops.push(ops);
                secs -= 1;
            }

            fsync(fd);
            close(fd);

            let _a = remove(self.path.as_ptr() as *const i8);
        }

        iops.clone()
    }
}

unsafe impl Sync for MIX {}
