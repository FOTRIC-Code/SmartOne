import type { ThemeTokens } from './designSetting';

/**
 * Inject theme tokens as CSS custom properties on :root so every component
 * can consume them via `var(--token-name)`.
 */
export function applyThemeVariables(tokens: ThemeTokens): void {
  const s = document.documentElement.style;

  // Layout
  s.setProperty('--header-height', `${tokens.layout.headerHeight}px`);
  s.setProperty('--sidebar-width', `${tokens.layout.sidebarWidth}px`);
  s.setProperty('--sidebar-collapsed-width', `${tokens.layout.sidebarCollapsedWidth}px`);
  s.setProperty('--tab-bar-height', `${tokens.layout.tabBarHeight}px`);
  s.setProperty('--content-margin', `${tokens.layout.contentMargin}px`);

  // Colors
  s.setProperty('--color-primary', tokens.colors.primary);
  s.setProperty('--color-primary-hover', tokens.colors.primaryHover);
  s.setProperty('--color-primary-pressed', tokens.colors.primaryPressed);
  s.setProperty('--color-text-base', tokens.colors.textBase);
  s.setProperty('--color-text-secondary', tokens.colors.textSecondary);
  s.setProperty('--color-text-muted', tokens.colors.textMuted);
  s.setProperty('--color-text-heading', tokens.colors.textHeading);
  s.setProperty('--color-page-bg', tokens.colors.pageBackground);
  s.setProperty('--color-card-bg', tokens.colors.cardBackground);
  s.setProperty('--color-border', tokens.colors.borderColor);
  s.setProperty('--color-border-light', tokens.colors.borderColorLight);
  s.setProperty('--color-warning', tokens.colors.warning);
  s.setProperty('--color-dark-bg', tokens.colors.darkBackground);
  s.setProperty('--color-shadow', tokens.colors.shadowColor);

  // Component tokens
  s.setProperty('--button-border-radius', tokens.components.button.borderRadius);
}
