mod aqua;

fn main() {
	let mut win = aqua::win::Win::new(800, 600);

	win.caption("Bob AQUA Skeleton");

	std::thread::sleep(std::time::Duration::from_millis(3000));
}
