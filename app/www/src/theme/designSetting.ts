import type { GlobalThemeOverrides } from 'naive-ui';

// ---------------------------------------------------------------------------
// Theme Token Types
// ---------------------------------------------------------------------------

export interface ThemeTokens {
  layout: {
    headerHeight: number;
    sidebarWidth: number;
    sidebarCollapsedWidth: number;
    tabBarHeight: number;
    contentMargin: number;
  };
  colors: {
    primary: string;
    primaryHover: string;
    primaryPressed: string;
    textBase: string;
    textSecondary: string;
    textMuted: string;
    textHeading: string;
    pageBackground: string;
    cardBackground: string;
    borderColor: string;
    borderColorLight: string;
    warning: string;
    darkBackground: string;
    shadowColor: string;
  };
  components: {
    button: { borderRadius: string };
  };
}

// ---------------------------------------------------------------------------
// Preset color list (unchanged)
// ---------------------------------------------------------------------------

export const appThemeList: string[] = [
  '#2d8cf0',
  '#0960bd',
  '#0084f4',
  '#009688',
  '#536dfe',
  '#ff5c93',
  '#ee4f12',
  '#0096c7',
  '#9c27b0',
  '#ff9800',
  '#FF3D68',
  '#00C1D4',
  '#71EFA3',
  '#171010',
  '#78DEC7',
  '#1768AC',
  '#FB9300',
  '#FC5404',
];

// ---------------------------------------------------------------------------
// Default theme tokens
// ---------------------------------------------------------------------------

export const themeTokens: ThemeTokens = {
  layout: {
    headerHeight: 64,
    sidebarWidth: 200,
    sidebarCollapsedWidth: 64,
    tabBarHeight: 32,
    contentMargin: 10,
  },
  colors: {
    primary: '#0f766e',
    primaryHover: '#0d9488',
    primaryPressed: '#115e59',
    textBase: '#44403c',
    textSecondary: '#78716c',
    textMuted: '#57534e',
    textHeading: '#1c1917',
    pageBackground: '#f3f1ec',
    cardBackground: '#fff',
    borderColor: '#e7e5e4',
    borderColorLight: '#f5f5f4',
    warning: '#c2410c',
    darkBackground: '#1c1917',
    shadowColor: '#dde0e0',
  },
  components: {
    button: { borderRadius: '8px' },
  },
};

/** Unified preview box for image / thermal settings (and bad-pixel display mapping). */
export const PREVIEW_VIDEO_SIZE = {
  width: 640,
  height: 480,
} as const;

/** @deprecated Use `themeTokens.layout` instead */
export const layoutDimensions = themeTokens.layout;

// ---------------------------------------------------------------------------
// Naive UI theme overrides generator
// ---------------------------------------------------------------------------

export function generateNaiveOverrides(tokens: ThemeTokens): GlobalThemeOverrides {
  return {
    common: {
      primaryColor: tokens.colors.primary,
      primaryColorHover: tokens.colors.primaryHover,
      primaryColorPressed: tokens.colors.primaryPressed,
      primaryColorSuppl: tokens.colors.primary,
    },
    Checkbox: {
      colorChecked: tokens.colors.primary,
    },
    Button: {
      borderRadiusMedium: tokens.components.button.borderRadius,
      borderRadiusLarge: tokens.components.button.borderRadius,
      borderRadiusSmall: tokens.components.button.borderRadius,
    },
    DataTable: {
      thColor: '#EBEEF5',
      thColorHover: '#EBEEF5',
      thColorSorting: '#EBEEF5',
    },
    Table: {
      thColor: '#EBEEF5',
    },
    LoadingBar: {
      colorLoading: tokens.colors.primary,
    },
  };
}

// ---------------------------------------------------------------------------
// Default export (backward-compatible)
// ---------------------------------------------------------------------------

const setting = {
  darkTheme: false,
  appTheme: '#2d8cf0',
  appThemeList,
};

export default setting;
