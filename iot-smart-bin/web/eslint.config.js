//  @ts-check

import { tanstackConfig } from "@tanstack/eslint-config";

export default [
  {
    ignores: ["eslint.config.js", "prettier.config.js"],
  },
  ...tanstackConfig,

  {
    rules: {
      "@typescript-eslint/array-type": "off",
    },
  },
];
