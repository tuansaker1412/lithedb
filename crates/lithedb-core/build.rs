use std::env;

fn main() {
    if let Ok(lib_dir) = env::var("DEP_SQLITE3_LIB_DIR") {
        println!("cargo:rustc-link-search=native={lib_dir}");
        println!("cargo:rustc-link-lib=static=sqlite3");
    }
}
