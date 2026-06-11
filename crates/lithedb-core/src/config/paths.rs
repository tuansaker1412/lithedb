use directories::ProjectDirs;
use std::fs;
use std::io;
use std::path::PathBuf;

const QUALIFIER: &str = "io.github";
const ORGANIZATION: &str = "tuansaker1412";
const APPLICATION: &str = "LitheDB";

fn current_project_dirs() -> io::Result<ProjectDirs> {
    ProjectDirs::from(QUALIFIER, ORGANIZATION, APPLICATION)
        .ok_or_else(|| io::Error::new(io::ErrorKind::NotFound, "XDG config dir not found"))
}

pub fn prepare_config_file(name: &str) -> io::Result<PathBuf> {
    let new_path = current_project_dirs()?.config_dir().join(name);
    if let Some(parent) = new_path.parent() {
        fs::create_dir_all(parent)?;
    }
    Ok(new_path)
}
