import 'package:flutter/material.dart';
import '../api/api_client.dart';
import 'home_screen.dart';

class LoginScreen extends StatefulWidget {
  final OzaynApiClient api;
  final VoidCallback onLoginSuccess;

  const LoginScreen({super.key, required this.api, required this.onLoginSuccess});

  @override
  State<LoginScreen> createState() => _LoginScreenState();
}

class _LoginScreenState extends State<LoginScreen> {
  final _userCtrl = TextEditingController();
  final _passCtrl = TextEditingController();
  final _emailCtrl = TextEditingController();
  bool _isRegister = false;
  bool _loading = false;
  String? _error;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: Center(
          child: SingleChildScrollView(
            padding: const EdgeInsets.all(32),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Container(
                  width: 80, height: 80,
                  decoration: BoxDecoration(
                    color: const Color(0xFF0A84FF).withOpacity(0.12),
                    borderRadius: BorderRadius.circular(40),
                  ),
                  child: const Center(
                    child: Text('O', style: TextStyle(fontSize: 40, fontWeight: FontWeight.bold, color: Color(0xFF0A84FF))),
                  ),
                ),
                const SizedBox(height: 16),
                const Text('Ozayn', style: TextStyle(fontSize: 32, fontWeight: FontWeight.bold, color: Color(0xFF0A84FF))),
                const Text('AI Digital Twin', style: TextStyle(fontSize: 14, color: Color(0x80EBEBF5))),
                const SizedBox(height: 40),
                if (_isRegister) ...[
                  Align(
                    alignment: Alignment.centerLeft,
                    child: TextButton(
                      onPressed: () => setState(() { _isRegister = false; _error = null; }),
                      child: const Text('< Back'),
                    ),
                  ),
                  const SizedBox(height: 8),
                ],
                TextField(
                  controller: _userCtrl,
                  decoration: const InputDecoration(hintText: 'Username'),
                ),
                const SizedBox(height: 12),
                if (_isRegister)
                  TextField(
                    controller: _emailCtrl,
                    decoration: const InputDecoration(hintText: 'Email (optional)'),
                  ),
                if (_isRegister) const SizedBox(height: 12),
                TextField(
                  controller: _passCtrl,
                  obscureText: true,
                  decoration: const InputDecoration(hintText: 'Password'),
                ),
                const SizedBox(height: 16),
                if (_error != null)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 12),
                    child: Text(_error!, style: const TextStyle(color: Color(0xFFFF453A), fontSize: 13)),
                  ),
                ElevatedButton(
                  onPressed: _loading ? null : _submit,
                  child: _loading
                      ? const SizedBox(width: 20, height: 20, child: CircularProgressIndicator(strokeWidth: 2, color: Colors.white))
                      : Text(_isRegister ? 'Create Account' : 'Sign In'),
                ),
                const SizedBox(height: 12),
                TextButton(
                  onPressed: () => setState(() { _isRegister = !_isRegister; _error = null; }),
                  child: Text(_isRegister ? 'Already have an account? Sign in' : 'Create Account'),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }

  Future<void> _submit() async {
    final u = _userCtrl.text.trim();
    final p = _passCtrl.text;
    if (u.isEmpty || p.isEmpty) {
      setState(() => _error = 'Please enter username and password');
      return;
    }
    setState(() { _loading = true; _error = null; });

    try {
      if (_isRegister) {
        final result = await widget.api.register(u, p, email: _emailCtrl.text.trim());
        if (result['success'] == true) {
          setState(() { _isRegister = false; _loading = false; });
          _error = null;
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Account created! Please sign in.')),
          );
        } else {
          setState(() { _error = result['error'] ?? 'Registration failed'; _loading = false; });
        }
      } else {
        final result = await widget.api.login(u, p);
        if (result['success'] == true) {
          widget.onLoginSuccess();
        } else {
          setState(() { _error = result['error'] ?? 'Login failed'; _loading = false; });
        }
      }
    } catch (e) {
      setState(() { _error = e.toString(); _loading = false; });
    }
  }

  @override
  void dispose() {
    _userCtrl.dispose();
    _passCtrl.dispose();
    _emailCtrl.dispose();
    super.dispose();
  }
}
