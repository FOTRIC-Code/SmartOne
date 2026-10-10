/** Windows 文件夹名非法字符：\ / : * ? " < > | 及控制字符 */
const WINDOWS_FOLDER_INVALID_CHARS = /[<>:"/\\|?*\x00-\x1f]/g;

/** 移除 Windows 文件夹名非法字符，用于设备名、通道名等会参与路径命名的字段 */
export function sanitizeFolderName(value: string) {
  return value.replace(WINDOWS_FOLDER_INVALID_CHARS, '');
}
