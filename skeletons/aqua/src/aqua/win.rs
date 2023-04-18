use aqua;

pub struct Win {
	dev: aqua::Device,
	win: u64,

	#[allow(dead_code)]
	x_res: u32,

	#[allow(dead_code)]
	y_res: u32,
}

impl Win {
	pub fn new(x_res: u32, y_res: u32) -> Win {
		// query aquabsd.alps.win device

		let dev = aqua::query_device("aquabsd.alps.win");

		// create window itself

		let win = aqua::send_device!(dev, 0x6377, x_res, y_res);

		Win {
			dev: dev,
			win: win,

			x_res: x_res,
			y_res: y_res,
		}
	}

	pub fn caption(&mut self, name: &str) {
		let c_str = std::ffi::CString::new(name).unwrap();
		aqua::send_device!(self.dev, 0x7363, self.win, c_str.as_ptr());
	}
}

impl Drop for Win {
	fn drop(&mut self) {
		aqua::send_device!(self.dev, 0x6463, self.win);
	}
}
