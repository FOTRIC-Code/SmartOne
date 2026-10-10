<template>
  <div ref="container" class="auto-context">
    <slot :scale="scale"></slot>
  </div>
</template>

<script setup>
  import { ref, onMounted, onBeforeUnmount, watch } from 'vue';

  const props = defineProps({
    width: {
      type: Number,
      required: true,
    },
    height: {
      type: Number,
      required: true,
    },
  });

  const container = ref(null);
  const scale = ref(1);

  const calculateScale = () => {
    if (!container.value) return;

    const rect = container.value.getBoundingClientRect();
    const scaleX = rect.width / props.width;
    const scaleY = rect.height / props.height;
    scale.value = Math.min(scaleX, scaleY);
  };

  let resizeObserver;

  onMounted(() => {
    if (container.value) {
      calculateScale();
      // 使用 ResizeObserver 监听容器大小变化
      resizeObserver = new ResizeObserver(calculateScale);
      resizeObserver.observe(container.value);
    }
  });

  onBeforeUnmount(() => {
    if (resizeObserver && container.value) {
      resizeObserver.unobserve(container.value);
      resizeObserver = null;
    }
  });

  // 当 width 或 height 变化时重新计算
  watch(
    () => [props.width, props.height],
    () => {
      calculateScale();
    }
  );
</script>

<style scoped>
  .auto-context {
    width: 100%;
    height: 100%;
    overflow: hidden;
    position: relative;
  }
</style>
