extern crate aqua;

#[no_mangle]
pub fn main() {
	let mut win = aqua::win::Win::new(800, 600);
	win.caption("Bob AQUA Skeleton");

	win.draw_loop(|| {
		0
	});
}
