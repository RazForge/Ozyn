import 'package:flutter/material.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'api/api_client.dart';
import 'screens/home_screen.dart';
import 'screens/login_screen.dart';
import 'theme/dark_theme.dart';

void main() {
  runApp(const OzaynApp());
}

class OzaynApp extends StatelessWidget {
  const OzaynApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Ozayn',
      theme: OzaynDarkTheme.theme,
      debugShowCheckedModeBanner: false,
      home: const AuthGate(),
    );
  }
}

class AuthGate extends StatefulWidget {
  const AuthGate({super.key});

  @override
  State<AuthGate> createState() => _AuthGateState();
}

class _AuthGateState extends State<AuthGate> {
  final _api = OzaynApiClient();
  bool _loading = true;
  bool _loggedIn = false;

  @override
  void initState() {
    super.initState();
    _checkSession();
  }

  Future<void> _checkSession() async {
    final prefs = await SharedPreferences.getInstance();
    final sessionId = prefs.getString('ozayn_session');
    if (sessionId != null) {
      _api.sessionId = sessionId;
      final valid = await _api.validateSession();
      if (valid) {
        setState(() {
          _loggedIn = true;
          _loading = false;
        });
        return;
      }
    }
    setState(() {
      _loggedIn = false;
      _loading = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    if (_loading) {
      return const Scaffold(
        body: Center(child: CircularProgressIndicator(color: Color(0xFF0A84FF))),
      );
    }
    if (_loggedIn) {
      return HomeScreen(api: _api, onLogout: _logout);
    }
    return LoginScreen(api: _api, onLoginSuccess: _onLogin);
  }

  void _onLogin() {
    setState(() => _loggedIn = true);
  }

  void _logout() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('ozayn_session');
    _api.sessionId = null;
    _api.user = null;
    setState(() => _loggedIn = false);
  }
}
