<template>
  <div
    class="flex items-center justify-center gap-[10px] separator-line-right px-[12px]"
    :style="{ minWidth: `var(--sidebar-width)`, height: `var(--header-height)` }"
  >
    <span class="brand-mark">SmartOne</span>
  </div>
</template>

<style lang="less" scoped>
  .brand-mark {
    font-size: 13px;
    font-weight: 700;
    letter-spacing: 0.22em;
    text-transform: uppercase;
    color: var(--color-primary);
  }
</style>
