use std::ffi::c_char;
use std::ffi::CString;

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("entry.h");
        unsafe fn entry(argc: i32, argv: *mut *mut c_char) -> i32;
    }
}

mod qt;

fn main() {
    let args = std::env::args().map(|arg| CString::new(arg).unwrap() ).collect::<Vec<CString>>();
    let mut c_args = args.iter().map(|arg| arg.clone().into_raw()).collect::<Vec<*mut c_char>>();
    unsafe {
        ffi::entry(c_args.len() as i32, c_args.as_mut_ptr());
    };
}
