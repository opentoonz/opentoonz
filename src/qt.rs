#[cxx_qt::bridge]
pub mod qt {
    unsafe extern "C++" {
        include!("sceneviewer.h");
        include!("intpairfield.h");
    }
}
