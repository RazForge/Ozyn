import 'package:flutter/material.dart';

class OzaynDarkTheme {
  static const Color bg = Color(0xFF08081A);
  static const Color surface = Color(0xFF161632);
  static const Color card = Color(0xFF1C1C38);
  static const Color accent = Color(0xFF0A84FF);
  static const Color text = Color(0xFFEBEBF5);
  static Color textFaded = const Color(0xFFEBEBF5).withOpacity(0.5);
  static const Color success = Color(0xFF30D158);
  static const Color danger = Color(0xFFFF453A);
  static const Color border = Color(0x1AFFFFFF);

  static ThemeData get theme => ThemeData(
    brightness: Brightness.dark,
    scaffoldBackgroundColor: bg,
    colorScheme: const ColorScheme.dark(
      primary: accent,
      surface: surface,
      onSurface: text,
    ),
    appBarTheme: AppBarTheme(
      backgroundColor: bg,
      foregroundColor: text,
      elevation: 0,
      centerTitle: false,
      titleTextStyle: const TextStyle(
        color: text,
        fontSize: 20,
        fontWeight: FontWeight.bold,
      ),
    ),
    cardTheme: CardTheme(
      color: card,
      elevation: 0,
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
        side: const BorderSide(color: border),
      ),
    ),
    inputDecorationTheme: InputDecorationTheme(
      filled: true,
      fillColor: const Color(0x33323255),
      border: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: border),
      ),
      enabledBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: border),
      ),
      focusedBorder: OutlineInputBorder(
        borderRadius: BorderRadius.circular(12),
        borderSide: const BorderSide(color: accent),
      ),
      hintStyle: TextStyle(color: textFaded),
    ),
    elevatedButtonTheme: ElevatedButtonThemeData(
      style: ElevatedButton.styleFrom(
        backgroundColor: accent,
        foregroundColor: Colors.white,
        minimumSize: const Size(double.infinity, 48),
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(12)),
        textStyle: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold),
      ),
    ),
    textButtonTheme: TextButtonThemeData(
      style: TextButton.styleFrom(foregroundColor: accent),
    ),
    listTileTheme: const ListTileThemeData(
      iconColor: textFaded,
      textColor: text,
    ),
  );
}
