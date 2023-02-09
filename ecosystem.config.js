module.exports = {
  apps : [{
    name: 'Prep_GPIO',
    script: '/home/raspberry/openFrameworks/apps/myApps/Gaugemancy/prep_GPIO.py',
    interpreter: 'python3',
    autorestart: false,
  },
  {
    name: "Gaugemancy",
    script: "cd /home/raspberry/openFrameworks/apps/myApps/Gaugemancy/bin &&./Gaugemancy",
    autorestart: false,
    exec_interpreter: "none",
    exec_mode : "fork_mode",
    wait_boot: 3000,
  }]
}