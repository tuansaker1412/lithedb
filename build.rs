use std::env;

fn main() {
    if let Ok(lib_dir) = env::var("DEP_SQLITE3_LIB_DIR") {
        println!("cargo:rustc-link-search=native={lib_dir}");
    }

    match env::var("DEP_SQLITE3_STATIC").as_deref() {
        Ok("1") | Ok("true") => println!("cargo:rustc-link-lib=static=sqlite3"),
        _ => println!("cargo:rustc-link-lib=sqlite3"),
    }
}
