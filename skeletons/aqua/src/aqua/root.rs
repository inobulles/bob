pub type Device = u64;

type KosQueryDevice = fn(u64, u64) -> Device;
type KosSendDevice = fn(u64, u64, u64, u64) -> u64;

static mut KOS_QUERY_DEVICE: Option<KosQueryDevice> = None;
static mut KOS_SEND_DEVICE: Option<KosSendDevice> = None;

#[no_mangle]
pub unsafe extern "C" fn aqua_set_kos_functions(kos_query_device: u64, kos_send_device: u64) {
	KOS_QUERY_DEVICE = Some(std::mem::transmute(kos_query_device));
	KOS_SEND_DEVICE = Some(std::mem::transmute(kos_send_device));
}

pub fn query_device(name: &str) -> Device {
	let c_str = std::ffi::CString::new(name).unwrap();

	unsafe {
		KOS_QUERY_DEVICE.unwrap()(0, c_str.as_ptr() as u64)
	}
}

pub fn raw_send_device(device: Device, cmd: u16, data: &mut [u64]) -> u64 {
	unsafe {
		KOS_SEND_DEVICE.unwrap()(0, device as u64, cmd as u64, data.as_ptr() as u64)
	}
}

#[macro_export]
macro_rules! send_device {
	($device: expr, $cmd: literal, $($data: expr),*) => {
		(|| -> u64 {
			let mut data = [$($data as u64),*];
			aqua::raw_send_device($device, $cmd, &mut data)
		})()
	};
}

// pub use ::send_device;
