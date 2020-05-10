use std::collections::HashMap;
use std::fs::OpenOptions;
use std::io::Write;
use std::path::Path;
use std::sync::{Arc, Barrier};
use std::thread;
use std::time::Duration;

mod utils;
use utils::topology::ThreadMapping;
use utils::topology::*;

mod bench;
use bench::DRBH;

pub trait Bench {
    fn init(&self);
    fn run(&self, b: Arc<Barrier>, duration: Duration) -> Vec<usize>;
}

struct BenchMark<T>
where
    T: Bench + Default + std::marker::Send + std::marker::Sync + 'static + std::clone::Clone,
{
    /// Thread assignments.
    thread_mappings: Vec<ThreadMapping>,
    /// # Threads.
    threads: Vec<usize>,
    ///
    bench: T,
}

impl<T> BenchMark<T>
where
    T: Bench + Default + std::marker::Send + std::marker::Sync + 'static + std::clone::Clone,
{
    pub fn new() -> BenchMark<T> {
        BenchMark {
            thread_mappings: Vec::new(),
            threads: Vec::new(),
            bench: Default::default(),
        }
    }

    pub fn thread_defaults(&mut self) -> &mut Self {
        let topology = MachineTopology::new();
        let max_cores = topology.cores();

        // On larger machines thread increments are bigger than on
        // smaller machines:
        let thread_incremements = if max_cores > 120 {
            8
        } else if max_cores > 24 {
            4
        } else if max_cores > 16 {
            4
        } else {
            2
        };

        for t in (0..(max_cores + 1)).step_by(thread_incremements) {
            if t == 0 {
                // Can't run on 0 threads
                self.threads(t + 1);
            } else {
                self.threads(t);
            }
        }

        /* Go in increments of one around "interesting" socket boundaries
        let sockets = topology.sockets();
        let cores_on_s0 = topology.cpus_on_socket(sockets[0]);
        let cores_per_socket = cores_on_s0.len();
        for i in 0..sockets.len() {
            let multiplier = i + 1;
            fn try_add(to_add: usize, max_cores: usize, cur_threads: &mut Vec<usize>) {
                if !cur_threads.contains(&to_add) && to_add <= max_cores {
                    cur_threads.push(to_add);
                }
            }

            let core_socket_boundary = multiplier * cores_per_socket;
            try_add(core_socket_boundary - 1, max_cores, &mut self.threads);
            try_add(core_socket_boundary, max_cores, &mut self.threads);
            try_add(core_socket_boundary + 1, max_cores, &mut self.threads);
        }*/

        self.threads.sort();
        self
    }

    /// Run benchmark with `t` threads.
    pub fn threads(&mut self, t: usize) -> &mut Self {
        self.threads.push(t);
        self
    }

    /// Run benchmark with given thread <-> machine mapping.
    pub fn thread_mapping(&mut self, tm: ThreadMapping) -> &mut Self {
        self.thread_mappings.push(tm);
        self
    }

    pub fn report_bench(&self, file_name: &str, ts: usize, result: &HashMap<u64, Vec<usize>>) {
        // Append parsed results to a CSV file
        let write_headers = !Path::new(file_name).exists();
        let mut csv_file = OpenOptions::new()
            .append(true)
            .create(true)
            .open(file_name)
            .expect("Can't open file");
        if write_headers {
            let row =
                "git_rev,thread_id,benchmark,ncores,memsize,duration_total,duration,operations\n";
            let r = csv_file.write(row.as_bytes());
            assert!(r.is_ok());
        }

        let mut result: Vec<(u64, Vec<usize>)> =
            result.iter().map(|(k, v)| (*k, v.clone())).collect();
        result.sort_by(|a, b| a.0.cmp(&b.0));
        for (core, iops) in result.iter() {
            let mut time = 1;
            for ops in iops {
                let r = csv_file.write(format!("{},", std::env!("GIT_HASH")).as_bytes());
                assert!(r.is_ok());
                let r = csv_file.write(
                    format!(
                        "{},{:?},{},{},{},{},{}",
                        *core, "drbh", ts, 4096, 10, time, ops
                    )
                    .as_bytes(),
                );
                assert!(r.is_ok());
                let r = csv_file.write("\n".as_bytes());
                assert!(r.is_ok());
                time += 1;
            }
        }
    }

    /// Start the benchmark
    pub fn start(&mut self) {
        let topology = MachineTopology::new();
        utils::disable_dvfs();
        let file_name = "fsops_benchmark.csv";
        let _ret = std::fs::remove_file(file_name);

        for tm in self.thread_mappings.iter() {
            for ts in self.threads.iter() {
                let cpus = topology.allocate(*tm, *ts, true);
                let cores: Vec<u64> = cpus.iter().map(|c| c.cpu).collect();
                let mut result: HashMap<u64, Vec<usize>> = HashMap::with_capacity(*ts);

                // Need a barrier to synchronize starting of threads
                let barrier = Arc::new(Barrier::new(*ts));
                let mut children = Vec::new();
                self.bench.init();

                for core in cores {
                    let b = barrier.clone();
                    let bench = self.bench.clone();

                    children.push(thread::spawn(move || {
                        utils::pin_thread(core);
                        (core, bench.run(b, Duration::from_secs(10)))
                    }));
                }

                // Wait for the thread to finish. Returns a result.
                for child in children {
                    let (core, iops) = child.join().unwrap();
                    result.insert(core, iops);
                }
                self.report_bench(file_name, *ts, &result);
            }
        }
    }
}

fn main() {
    BenchMark::<DRBH>::new()
        .thread_defaults()
        .thread_mapping(ThreadMapping::Interleave)
        .start();
}
