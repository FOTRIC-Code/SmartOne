import './theme/styles/tailwind.css';
import './theme/styles/index.less';
import { createApp } from 'vue';
import {
  setupNaiveDiscreteApi,
  setupNaive,
  setupGlobalMethods,
  setupCustomComponents,
} from '@/manager/plugins';
import App from './App.vue';
import router, { setupRouter } from './routes';
import { setupStore } from '@/data';

async function bootstrap() {
  const app = createApp(App);

  setupStore(app);
  setupNaive(app);
  setupNaiveDiscreteApi();
  setupCustomComponents(app);
  await setupGlobalMethods(app);
  setupRouter(app);
  await router.isReady();

  const meta = document.createElement('meta');
  meta.name = 'naive-ui-style';
  document.head.appendChild(meta);

  app.mount('#app', true);
}

void bootstrap();
