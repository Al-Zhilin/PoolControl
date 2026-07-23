# This script transfer OTA secrets from pass.h to build_flag`s in platformio.ini

Import("env")
import re

def get_define(content, name):
    m = re.search(rf'#define\s+{name}\s+"([^"]*)"', content)
    return m.group(1) if m else ""

def before_upload(source, target, env):
    with open("pass.h") as f:
        content = f.read()
    ota_name = get_define(content, "OTA_NAME")
    ota_pass = get_define(content, "OTA_PASS")
    env.Replace(UPLOAD_PORT=f"{ota_name}.local")
    env.Append(UPLOADERFLAGS=[f"--auth={ota_pass}"])

    # Если в pass.h сменились данные дял OTA, то прошивать нужно с вручную введенными старыми данными
    # т.к. ESP до прошивки новых данных все еще отзывается на старые:

    # env.Replace(UPLOAD_PORT="*****.local")
    # env.Append(UPLOADERFLAGS=["--auth=********"])

env.AddPreAction("upload", before_upload)
