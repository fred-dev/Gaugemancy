
module.exports = {
  apps : [{
    name: "Gaugemancy",
    script: "cd /home/raspberry/openFrameworks/apps/myApps/Gaugemancy/bin &&./Gaugemancy",
    autorestart: false,
    exec_interpreter: "none",
    exec_mode : "fork_mode",
    wait_boot: 3000,
   env: {
      PATH: process.env.PATH,
      GPIO_PATH: "/sys/class/gpio"
    }
  }]
}
