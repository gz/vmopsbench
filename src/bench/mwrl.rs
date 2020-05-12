use crate::Bench;
use libc::*;
use std::sync::{Arc, Barrier};
use std::time::{Duration, Instant};

#[derive(Clone)]
pub struct MWRL {
    path: &'static str,
}

unsafe impl Sync for MWRL {}

impl Default for MWRL {
    fn default() -> MWRL {
        MWRL {
            // It doesn't work if trailing \0 isn't there in the filename.
            path: "/mnt",
        }
    }
}

impl Bench for MWRL {
    fn init(&self, cores: Vec<u64>) {
        unsafe {
            for core in cores {
                let dir_name = format!("{}/{}/\0", self.path, core);
                if mkdir(dir_name.as_ptr() as *const i8, S_IRWXU) != 0 {
                    panic!("MWRL: Unable to create directory {}", nix::errno::errno());
                }
                let filename = format!("{}/{}/file-0.txt\0", self.path, core);

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

    fn run(&self, b: Arc<Barrier>, duration: u64, core: u64) -> Vec<usize> {
        let mut secs = duration as usize;
        let mut iops = Vec::with_capacity(secs);

        unsafe {
            b.wait();

            let mut iter = 0;
            while secs > 0 {
                let mut ops = 0;
                let start = Instant::now();
                let end_experiment = start + Duration::from_secs(1);
                while Instant::now() < end_experiment {
                    for _i in 0..128 {
                        let old_name = format!("{}/{}/file-{}.txt\0", self.path, core, iter);
                        iter += 1;
                        let new_name = format!("{}/{}/file-{}.txt\0", self.path, core, iter);
                        if rename(
                            old_name.as_ptr() as *const i8,
                            new_name.as_ptr() as *const i8,
                        ) != 0
                        {
                            panic!(
                                "MWRL: Unable to rename file, errno: {}",
                                nix::errno::errno()
                            );
                        }
                        ops += 1;
                    }
                }
                iops.push(ops);
                secs -= 1;
            }

            b.wait();
            let filename = format!("{}/{}/file-{}.txt\0", self.path, core, iter);
            if remove(filename.as_ptr() as *const i8) != 0 {
                panic!(
                    "MWRL: Unable to remove file, errno: {}",
                    nix::errno::errno()
                );
            }

            let dir_name = format!("{}/{}/\0", self.path, core);
            if rmdir(dir_name.as_ptr() as *const i8) != 0 {
                panic!(
                    "MWRL: Unable to remove file, errno: {}",
                    nix::errno::errno()
                );
            }
        }

        iops.clone()
    }
}
