import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "greenpico control",
  description: "Local control panel for the Pico Clock Green",
};

export default function RootLayout({ children }: Readonly<{ children: React.ReactNode }>) {
  return (
    <html lang="en">
      <body>{children}</body>
    </html>
  );
}
