import dayjs from 'dayjs';
import 'dayjs/locale/zh-cn';
import timezone from 'dayjs/plugin/timezone';
import utc from 'dayjs/plugin/utc';

dayjs.locale('zh-cn');
dayjs.extend(utc);
dayjs.extend(timezone);

export { dayjs };
